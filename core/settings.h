#pragma once

#include "utils.h"
#include <set>
#include <unordered_map>

class SettingType {
public:
	virtual bool deSerialize(cstr_t &str)
	{
		// "true" on success
		return false;
	}

	virtual std::string serialize() const
	{
		return "<invalid>";
	}
};


class Settings;

class Settings {
public:
	Settings(cstr_t &filename, Settings *parent = nullptr, cstr_t &prefix = "");
	~Settings();

	cstr_t &get(cstr_t &key) const;
	void set(cstr_t &key, cstr_t &value);
	bool get(cstr_t &key, SettingType *type) const;
	void set(cstr_t &key, SettingType *type);
	bool remove(cstr_t &key);

	bool syncFileContents();

private:
	std::mutex m_lock;

	std::string m_file;
	Settings *m_parent = nullptr;
	// To access only certain settings
	std::string *m_prefix = nullptr;

	std::unordered_map<std::string, std::string> m_settings;
	// List of modified entries since last save
	std::set<std::string> m_modified;
};


class SettingTypeLong : public SettingType {
public:
	SettingTypeLong(long init = 0) : value(init) {}

	bool deSerialize(cstr_t &str)
	{
		return sscanf(str.c_str(), "%li", &value) == 1;
	}

	std::string serialize() const
	{
		return std::to_string(value);
	}

	long value;
};
