#pragma once

#include "container.h"
#include "types.h"
#include "utils.h"
#include <set>

//#include <iostream>

class Client;
class Channel;
class IUserOwner;

// This is network-wide

class UserInstance : public Containers {
public:
	UserInstance(cstr_t &name);
	int getRefs() const { return m_references; }

	std::string nickname;
	std::string hostmask;

	// Valid for ACC and STATUS
	enum UserAccStatus {
		UAS_UNKNOWN = -1,
		UAS_NONE = 0,
		UAS_EXISTS = 1,
		UAS_RECOGNIZED = 2,
		UAS_LOGGED_IN = 3
	};
	UserAccStatus account = UAS_UNKNOWN;

private:
	friend class IUserOwner;
	friend class Channel;

	void grabRef()
	{
		m_references++;
	}
	void dropRef()
	{
		m_references--;
		if (m_references <= 0)
			delete this;
	}

	int m_references;
};

class IUserOwner {
public:
	IUserOwner(Client *cli);
	virtual ~IUserOwner();

	UserInstance *addUser(cstr_t &name);
	UserInstance *getUser(cstr_t &name) const;
	bool removeUser(UserInstance *ui);

	std::set<UserInstance *> &getAllUsers()
	{ return m_users; }

protected:
	Client *m_client = nullptr;

	// Per-user data
	std::set<UserInstance *> m_users;
};

class Channel : public IUserOwner {
public:
	Channel(cstr_t &name, Client *cli);
	~Channel();

	cstr_t &getName() const
	{ return m_name; }

	bool isPrivate() const
	{ return isPrivate(m_name); }

	static bool isPrivate(cstr_t &name)
	{ return name[0] != '#'; }

	Containers *getContainers()
	{ return m_containers; }

	void say(cstr_t &text);
	void notice(UserInstance *ui, cstr_t &text);
	void leave();
private:
	std::string m_name;

	// Per-channel data
	Containers *m_containers = nullptr;
};


class Network : public ICallbackHandler, public IUserOwner {
public:
	Network(Client *cli) : IUserOwner(cli) {}
	~Network();

	void setActiveChannel(cstr_t &name)
	{ m_last_active_channel = name; }

	Channel *addChannel(cstr_t &name);
	Channel *getChannel(cstr_t &name) const;
	Channel *getChannel() const;
	bool removeChannel(Channel *c);

	std::set<Channel *> &getAllChannels()
	{ return m_channels; }

private:
	std::string m_last_active_channel;

	std::set<Channel *> m_channels;
};
