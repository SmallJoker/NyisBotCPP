#pragma once

#include <map>
#include <string>
#include <vector>

class UserInstance;
class Channel;

class ICallbackHandler {
public:
	virtual ~ICallbackHandler() {}

	struct ChatInfo {
		UserInstance *ui;
		std::string text;
	};

	virtual void onClientReady() {}
	virtual void onChannelJoin(Channel *c) {}
	virtual void onChannelLeave(Channel *c) {}
	virtual void onUserJoin(Channel *c, UserInstance *ui) {}
	virtual void onUserLeave(Channel *c, UserInstance *ui) {}
	virtual void onUserRename(UserInstance *ui, const std::string &old_name) {}
	virtual bool onUserSay(Channel *c, ChatInfo info) { return false; }
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

