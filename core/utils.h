#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

typedef std::unique_lock<std::mutex> MutexAutoLock;
typedef const std::string cstr_t;


std::string trim(cstr_t &str);
std::vector<std::string> strsplit(cstr_t &input, char delim = ' ');
std::string get_next_part(std::string &input);

class UserInstance;
class Channel;

class ICallbackHandler {
public:
	virtual ~ICallbackHandler() {}

	struct ChatInfo {
		UserInstance *ui;
		std::vector<std::string> parts;
	};

	virtual void onChannelJoin(Channel *c) {}
	virtual void onChannelLeave(Channel *c) {}
	virtual void onUserJoin(Channel *c, UserInstance *ui) {}
	virtual void onUserLeave(Channel *c, UserInstance *ui) {}
	virtual void onUserRename(Channel *c, UserInstance *ui, cstr_t &old_name) {}
	virtual bool onUserSay(Channel *c, const ChatInfo &info) { return false; }
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
