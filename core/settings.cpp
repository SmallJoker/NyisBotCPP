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
	VERBOSE("file=" << m_file << ", prefix=" << (m_prefix ? *m_prefix : "(null)"));
}

Settings::~Settings()
{
	VERBOSE("file=" << m_file << ", prefix=" << (m_prefix ? *m_prefix : "(null)"));
	delete m_prefix;
}


#define KEY_ABS(key) (m_prefix ? *m_prefix + key : key)

Settings *Settings::fork(cstr_t &prefix)
{
	Settings *s = new Settings(m_file, this, KEY_ABS(prefix));
	s->m_is_fork = true;
	return s;
}

//  ================= Utility functions  =================

bool Settings::isKeyValid(cstr_t &str)
{
	if (str.empty())
		return false;

	for (char c : str) {
		if (!isKeyCharValid(c))
			return false;
	}
	return true;
}

void Settings::sanitizeKey(std::string &key)
{
	std::string blacklist(key.size(), '\0');
	size_t b = 0, k = 0;
	for (size_t i = 0; i < key.size(); ++i) {
		if (!isKeyCharValid(key[i]) || key[i] == '.')
			blacklist[b++] = key[i];
		else
			key[k++] = key[i];
	}
	if (k == key.size())
		return; // None

	uint32_t hash = hashELF32(&blacklist[0], b);
	key.resize(k + 1 + 8);
	snprintf(&key[k], key.size() - k + 1, "_%08X", hash);
}

void Settings::sanitizeValue(std::string &value)
{
	for (char &c : value) {
		if (iscntrl(c))
			c = ' ';
	}
}


// ================= get, set, remove =================

static std::string unknown_setting = "";
cstr_t &Settings::getAbsolute(cstr_t &key) const
{
	if (m_is_fork)
		return m_parent->getAbsolute(key);

	MutexLock _(m_lock);
	auto it = m_settings.find(key);
	if (it != m_settings.end())
		return it->second;

	if (m_parent)
		return m_parent->getAbsolute(key);

	WARN("Attempt to access unknown setting " << key);
	return unknown_setting;
}

bool Settings::setAbsolute(cstr_t &key, cstr_t &value)
{
	if (m_is_fork)
		return m_parent->setAbsolute(key, value);

	if (!isKeyValid(key)) {
		WARN("Invalid key '" << key << "'");
		return false;
	}

	MutexLock _(m_lock);

	m_modified.insert(key);
	m_settings[key] = value;
	sanitizeValue(m_settings[key]);
	return true;
}

bool Settings::removeAbsolute(cstr_t &key)
{
	if (m_is_fork)
		return m_parent->removeAbsolute(key);

	MutexLock _(m_lock);

	auto it = m_settings.find(key);
	if (it == m_settings.end())
		return false;

	m_modified.insert(key);
	m_settings.erase(it);
	return true;
}

cstr_t &Settings::get(cstr_t &key) const
{
	return getAbsolute(KEY_ABS(key));
}

bool Settings::set(cstr_t &key, cstr_t &value)
{
	return setAbsolute(KEY_ABS(key), value);
}

bool Settings::remove(cstr_t &key)
{
	return removeAbsolute(KEY_ABS(key));
}


// ================= SettingType-specific =================

bool Settings::get(cstr_t &key, SettingType *type) const
{
	cstr_t &val = getAbsolute(KEY_ABS(key));
	return type->deSerialize(val);
}

bool Settings::set(cstr_t &key, SettingType *type)
{
	return setAbsolute(KEY_ABS(key), type->serialize());
}

#undef KEY_ABS

std::vector<std::string> Settings::getKeys() const
{
	// Choose the appropriate members to access
	auto *mutex = &m_lock;
	const auto *map = &m_settings;
	if (m_is_fork) {
		mutex = &m_parent->m_lock;
		map = &m_parent->m_settings;
	}
	MutexLock _(*mutex);

	std::vector<std::string> keys;
	for (const auto &it : *map) {
		if (m_prefix && it.first.find(*m_prefix) != 0)
			continue;

		if (m_prefix)
			keys.push_back(it.first.substr(m_prefix->size()));
		else
			keys.push_back(it.first);
	}
	return keys;
}

bool Settings::syncFileContents(SyncReason reason)
{
	if (m_is_fork)
		return m_parent->syncFileContents(reason);

	MutexLock _(m_lock);

	std::ifstream is(m_file);
	if (!is.good()) {
		if (reason == SR_READ) {
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

	if (m_modified.empty() && reason == SR_WRITE)
		return true; // Nothing to do

	std::string new_file = m_file + ".~new";
	std::ofstream *of = (!m_modified.empty() && reason != SR_READ) ?
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
			if (pos == std::string::npos || pos == 0) {
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

