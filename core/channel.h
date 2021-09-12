#pragma once

#include "utils.h"
#include <set>

#include <iostream>

class Client;
class Channel;

class UserInstance {
public:
	UserInstance(cstr_t &name)
	{
		nickname = name;
		data = new Containers();
		m_references = 0;
	}
	~UserInstance()
	{
		//std::cout << "Deleted " << nickname << std::endl;
		delete data;
		data = nullptr;
	}
	void grab()
	{
		m_references++;
	}
	void drop()
	{
		m_references--;
		if (m_references <= 0)
			delete this;
	}
	int getRefs() const { return m_references; }

	std::string nickname;
	std::string hostmask;
	Containers *data;

private:
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

	Channel *getOrCreateChannel(cstr_t &name);
	Channel *getChannel();
	bool removeChannel(Channel *c);

	std::set<Channel *> &getAllChannels()
	{ return m_channels; }

private:
	std::string m_last_active_channel;

	std::set<Channel *> m_channels;
	std::set<UserInstance *> m_users;
};
