#include "settings.h"
#include "logger.h"
#include <sstream>
#include <fstream>

bool SettingType::parseLong(cstr_t &str, long *v, int base)
{
	const char *start = str.c_str();
	return parseLong(&start, v, base);
}

bool SettingType::parseLong(const char **pos, long *v, int base)
{
	char *endp = nullptr;
	long val = strtol(*pos, &endp, base);
	bool read = endp && endp != *pos;
	*pos = endp;
	if (read)
		*v = val;
	return read;
}

bool SettingType::parseFloat(const char **pos, float *v)
{
	char *endp = nullptr;
	float val = strtof(*pos, &endp);
	bool read = endp && endp != *pos;
	*pos = endp;
	if (read)
		*v = val;
	return read;
}

Settings::Settings(cstr_t &filename, Settings *parent, cstr_t &prefix)
{
	m_file = filename;
	m_parent = parent;

	if (!prefix.empty())
		m_prefix = new std::string(prefix + '.');
}

Settings::~Settings()
{
	delete m_prefix;
}

Settings *Settings::fork(cstr_t &prefix)
{
	return new Settings(m_file, m_parent, m_prefix ? *m_prefix + prefix : prefix);
}

static std::string unknown_setting = "";
	
bool Settings::isKeyValid(cstr_t &str)
{
	if (str.empty())
		return false;

	for (char c : str) {
		if (c >= 'A' && c <= 'Z')
			continue;
		if (c >= 'a' && c <= 'z')
			continue;
		if (c >= '0' && c <= '9')
			continue;
		if (c == '_' || c == '.')
			continue;

		return false;
	}
	return true;
}

#define KEY_RAW(key) (m_prefix ? *m_prefix + key : key)

cstr_t &Settings::get(cstr_t &key) const
{
	auto it = m_settings.find(KEY_RAW(key));
	if (it != m_settings.end())
		return it->second;

	if (m_parent)
		return m_parent->get(KEY_RAW(key));

	WARN("Attempt to access unknown setting " << KEY_RAW(key));
	return unknown_setting;
}

bool Settings::set(cstr_t &key, cstr_t &value)
{
	if (!isKeyValid(key)) {
		WARN("Invalid key '" << key << "'");
		return false;
	}

	MutexLock _(m_lock);

	std::string keyp = KEY_RAW(key);
	m_modified.insert(keyp);
	m_settings[keyp] = value;
	return true;
}


bool Settings::get(cstr_t &key, SettingType *type) const
{
	cstr_t &val = get(key);
	return type->deSerialize(val);
}

bool Settings::set(cstr_t &key, SettingType *type)
{
	return set(key, type->serialize());
}

std::vector<std::string> Settings::getKeys() const
{
	std::vector<std::string> keys;
	for (const auto &it : m_settings) {
		if (m_prefix && it.first.find(*m_prefix) != 0)
			continue;

		if (m_prefix)
			keys.push_back(it.first.substr(m_prefix->size()));
		else
			keys.push_back(it.first);
	}
	return keys;
}

bool Settings::remove(cstr_t &key)
{
	std::string keyp = KEY_RAW(key);
	MutexLock _(m_lock);

	auto it = m_settings.find(keyp);
	if (it == m_settings.end())
		return false;

	m_modified.insert(keyp);
	m_settings.erase(it);
	return true;
}


bool Settings::syncFileContents()
{
	MutexLock _(m_lock);

	std::ifstream is(m_file);
	if (!is.good()) {
		if (m_modified.size() == 0) {
			WARN("File '" << m_file << "' not found");
			return false;
		}

		// Create new file
		std::ofstream os(m_file);
		if (!os.good()) {
			ERROR("Failed to create file: '" << m_file << "'");
			return false;
		}

		os.close();
		is = std::ifstream(m_file);
	}

	std::string new_file = m_file + ".~new";
	std::ofstream *of = m_modified.size() ?
		new std::ofstream(new_file) : nullptr;

	// List of keys to detect removed settings
	std::set<std::string> all_keys;

	int line_n = 0;

	while (is.good() && ++line_n) {
		std::string line;
		std::getline(is, line);

		enum {
			LS_KEEP,
			LS_TODO,
			LS_HANDLED
		} line_status = LS_TODO;

		do {
			line_status = LS_KEEP;
			for (char c : line) {
				if (std::isspace(c))
					continue;

				// If not starting with comment: mark for parsing
				if (c != '#')
					line_status = LS_TODO;
				break;
			}
			if (line_status == LS_KEEP)
				break;

			size_t pos = line.find_first_of('=');
			if (pos == std::string::npos || pos == 0 || pos + 1 == line.size()) {
				WARN("Syntax error in line " << line_n << ": " << line);
				break;
			}

			std::string key = strtrim(line.substr(0, pos - 1));
			if (!isKeyValid(key)) {
				WARN("Invalid key '" << key << " in line " << line_n);
				break;
			}

			if (m_prefix && key.rfind(*m_prefix, 0) != 0) {
				// Not our business
				line_status = LS_KEEP;
				break;
			}
			all_keys.insert(key);

			if (m_modified.find(key) != m_modified.end()) {
				// Modified setting
				m_modified.erase(key);
				line_status = LS_HANDLED;

				auto it = m_settings.find(key);
				if (it == m_settings.end()) {
					// Setting removed. Skip line
					break;
				}

				// New setting value
				*of << key << " = " << it->second << std::endl;
				break;
			}

			// Read and keep value if the prefix matches
			std::string value = strtrim(line.substr(pos + 1));
			m_settings[key] = value;
			line_status = LS_KEEP; // Use line as-is
		} while (false);

		if (line_status != LS_HANDLED && of) {
			if (line_status == LS_TODO)
				*of << '#'; // Comment erroneous lines

			if (line.size() > 0)
				*of << line << std::endl;
		}
	}

	// Append new settings
	for (cstr_t &key : m_modified)
		*of << key << " = " << m_settings[key] << std::endl;

	// Remove removed values
	for (auto kv = m_settings.cbegin(); kv != m_settings.cend(); /*NOP*/) {
		auto it = all_keys.find(kv->first);
		if (it != all_keys.end()) {
			++kv;
			continue; // OK
		}

		// Remove missing value
		m_settings.erase(kv++);
	}

	m_modified.clear();
	is.close();

	if (of) {
		// Write new contents and replace existing file
		of->flush();
		of->close();
		delete of;

		std::remove(m_file.c_str());
		std::rename(new_file.c_str(), m_file.c_str());
	}
	return true;
}

#undef KEY_RAW

