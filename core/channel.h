#pragma once

#include "utils.h"
#include <set>

class Client;
class Channel;

struct UserInstance {
	UserInstance(cstr_t &name)
	{
		nickname = name;
		data = new Containers();
	}
	~UserInstance()
	{
		delete data;
	}

	std::string nickname;
	std::string hostmask;
	Containers *data;
};

class Channel {
public:
	Channel(cstr_t &name);
	~Channel();

	cstr_t &getName() const
	{ return m_name; }

	bool isPrivate() const
	{ return isPrivate(m_name); }

	static bool isPrivate(cstr_t &name)
	{ return name[0] != '#'; }

	Containers *getContainers()
	{ return m_containers; }

	UserInstance *addUser(cstr_t &name);
	UserInstance *getUser(cstr_t &name) const;
	bool removeUser(UserInstance *ui);

	std::set<UserInstance *> &getAllUsers()
	{ return m_users; }

	void say(cstr_t &what);
	void quit();
private:
	std::string m_name;

	// Per-user data
	std::set<UserInstance *> m_users;
	// Per-channel data
	Containers *m_containers;
};



class Network : public ICallbackHandler {
public:
	Network(Client *cli) :
		m_client(cli) {}

	void setActiveChannel(cstr_t &name)
	{ m_last_active_channel = name; }

	Channel *getChannel();
	Channel *getOrCreateChannel(cstr_t &name);

	std::set<Channel *> &getAllChannels()
	{ return m_channels; }

private:
	std::string m_last_active_channel;

	Client *m_client;
	std::set<Channel *> m_channels;
	std::set<UserInstance *> m_users;
};
