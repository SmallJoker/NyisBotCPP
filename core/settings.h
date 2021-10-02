#pragma once

#include "types.h"
#include "utils.h"
#include <set>
#include <unordered_map>

struct SettingType {
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

	static bool parseLong(cstr_t &str, long *out, int base = 10);
	static bool parseLong(const char **str, long *v, int base = 10);
	static bool parseFloat(const char **str, float *v);
};

enum SyncReason {
	SR_READ,  // Skips writing
	SR_WRITE, // Skips reading if nothing was modified
	SR_BOTH   // Read file & write if necessary
};

class Settings;

class Settings {
public:
	Settings(cstr_t &filename, Settings *parent = nullptr, cstr_t &prefix = "");
	~Settings();
	DISABLE_COPY(Settings);

	Settings *fork(cstr_t &prefix);

	cstr_t &get(cstr_t &key) const;
	bool set(cstr_t &key, cstr_t &value);
	bool remove(cstr_t &key);
	bool get(cstr_t &key, SettingType *type) const;
	bool set(cstr_t &key, SettingType *type);
	std::vector<std::string> getKeys() const;

	bool syncFileContents(SyncReason reason = SR_BOTH);

	static bool isKeyValid(cstr_t &key);
	static void sanitizeKey(std::string &key);

private:
	cstr_t &getAbsolute(cstr_t &key) const;
	bool    setAbsolute(cstr_t &key, cstr_t &value);
	bool removeAbsolute(cstr_t &key);
	void sanitizeValue(std::string &value);

	inline static bool isKeyCharValid(char c)
	{
		return (c >= 'A' && c <= 'Z')
			|| (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9')
			|| (c == '_' || c == '.');
	}

	mutable std::mutex m_lock;

	std::string m_file;
	Settings *m_parent = nullptr;
	// To access only certain settings
	std::string *m_prefix = nullptr;

	bool m_is_fork = false;
	std::unordered_map<std::string, std::string> m_settings;
	// List of modified entries since last save
	std::set<std::string> m_modified;
};
