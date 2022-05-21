#pragma once

#include "clientrequest.h"
#include "types.h"
#include <map>
#include <vector>

class UserInstance;
class Channel;

struct ContainerOwner {};

class ICallbackHandler : public ContainerOwner {
public:
	virtual ~ICallbackHandler() {}

	virtual void onChannelJoin(Channel *c) {}
	virtual void onChannelLeave(Channel *c) {}
	virtual void onUserJoin(Channel *c, UserInstance *ui) {}
	virtual void onUserLeave(Channel *c, UserInstance *ui) {}
	virtual void onUserRename(UserInstance *ui, const std::string &old_name) {}
	virtual bool onUserSay(Channel *c, UserInstance *ui, std::string &msg) { return false; }
	virtual void onUserStatusUpdate(UserInstance *ui, bool is_timeout) {}
};


struct IContainer : public ContainerOwner {
	IContainer() = default;
	virtual ~IContainer() {}
	DISABLE_COPY(IContainer);

	virtual std::string dump() const { return "??"; }
};

class Containers {
public:
	Containers() = default;
	~Containers();
	DISABLE_COPY(Containers);

	// Adds a new or replaces an existing container
	void set(const ContainerOwner *owner, IContainer *data);
	IContainer *get(const ContainerOwner *owner) const;
	bool remove(const ContainerOwner *owner);
	bool move(const ContainerOwner *old_owner, const ContainerOwner *new_owner);
	inline size_t size() const { return m_data.size(); }

private:
	std::map<const ContainerOwner *, IContainer *> m_data;
};

