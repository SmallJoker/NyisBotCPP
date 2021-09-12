#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

typedef std::unique_lock<std::mutex> MutexAutoLock;
typedef const std::string cstr_t;


std::string trim(cstr_t &str);
std::vector<std::string> strsplit(cstr_t &input, char delim);

class UserInstance;

class ICallbackHandler {
public:
	struct ChatInfo {
		UserInstance *ui;
		std::vector<std::string> parts;
	};

	virtual void onUserJoin(UserInstance *ui) {}
	virtual void onUserLeave(UserInstance *ui) {}
	virtual void onUserRename(UserInstance *ui, cstr_t &old_name) {}
	virtual void onUserSay(const ChatInfo &info) {}
};


struct IContainer {
	virtual ~IContainer() {}
	virtual std::string dump() const { return "??"; }
};

class Containers {
public:
	~Containers();

	bool add(const void *owner, IContainer *data);
	IContainer *get(const void *owner) const;
	bool remove(const void *owner);
	bool move(const void *old_owner, const void *new_owner);

protected:
	std::map<const void *, IContainer *> m_data;
};
