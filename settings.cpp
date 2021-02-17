#include "inc/settings.h"
#include "inc/logger.h"
#include <sstream>
#include <fstream>

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

static std::string unknown_setting = "";
	
static bool is_valid_key(cstr_t &str)
{
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


cstr_t &Settings::get(cstr_t &key) const
{
	auto it = m_settings.find(m_prefix ? *m_prefix + key : key);
	if (it != m_settings.end())
		return it->second;

	if (m_parent)
		return m_parent->get(key);

	WARN("Attempt to access unknown setting " << key);
	return unknown_setting;
}

void Settings::set(cstr_t &key, cstr_t &value)
{
	if (!is_valid_key(key)) {
		WARN("Invalid key '" << key << "'");
		return;
	}

	MutexAutoLock _(m_lock);

	std::string keyp = m_prefix ? *m_prefix + key : key;
	m_modified.insert(keyp);
	m_settings[keyp] = value;
}


bool Settings::get(cstr_t &key, SettingType *type) const
{
	cstr_t &val = get(key);
	return type->deSerialize(val);
}

void Settings::set(cstr_t &key, SettingType *type)
{
	set(key, type->serialize());
}


bool Settings::syncFileContents()
{
	MutexAutoLock _(m_lock);

	std::ifstream is(m_file);
	if (!is.good()) {
		WARN("File '" << m_file << "' not found");
		return false;
	}

	std::string new_file = m_file + ".~new";
	std::ofstream *of = m_modified.size() ?
		new std::ofstream(new_file) : nullptr;

	int line_n = 0;

	while (is.good() && ++line_n) {
		std::string line;
		std::getline(is, line);

		enum {
			LS_KEEP,
			LS_ERROR,
			LS_HANDLED
		} line_status = LS_ERROR;

		do {
			for (char c : line) {
				if (std::isspace(c))
					continue;

				// Comment starting after optional whitespaces?
				if (c == '#')
					line_status = LS_KEEP;
				break;
			}
			if (line_status == LS_KEEP)
				break;

			size_t pos = line.find_first_of('=');
			if (pos == std::string::npos || pos == 0 || pos + 1 == line.size()) {
				WARN("Syntax error in line " << line_n);
				break;
			}

			std::string key = trim(line.substr(0, pos - 1));
			if (!is_valid_key(key)) {
				WARN("Invalid key '" << key << " in line " << line_n);
				break;
			}

			if (m_prefix && key.rfind(*m_prefix, 0) == 0) {
				// Not our business
				line_status = LS_KEEP;
				break;
			}
				
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
			std::string value = trim(line.substr(pos + 1));
			m_settings[key] = value;
			line_status = LS_KEEP;
		} while (false);

		if (line_status != LS_HANDLED && of) {
			if (line_status == LS_ERROR)
				*of << '#'; // Comment erroneous lines

			// TODO: does "line" include \n or \r\n?
			*of << line << std::endl;
		}
	}

	// Append new settings
	for (cstr_t &key : m_modified)
		*of << key << " = " << m_settings[key] << std::endl;

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



