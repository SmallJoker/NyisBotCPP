#pragma once

#include "container.h"
#include "types.h"
#include "utils.h"
#include <set>

//#include <iostream>

class Channel;
class IClient;
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
		UAS_PENDING = -2,
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
	IUserOwner(IClient *cli);
	virtual ~IUserOwner();

	UserInstance *addUser(cstr_t &name);
	UserInstance *getUser(cstr_t &name) const;
	bool removeUser(UserInstance *ui);
	bool contains(UserInstance *ui) const;

	std::set<UserInstance *> &getAllUsers()
	{ return m_users; }

protected:
	IClient *m_client = nullptr;

	// Per-user data
	std::set<UserInstance *> m_users;
};

class Channel : public IUserOwner {
public:
	Channel(cstr_t &name, IClient *cli);
	~Channel();

	cstr_t &getName() const
	{ return m_name; }

	// Settings-safe name
	cstr_t &getSettingsName() const
	{ return m_name_settings; }

	bool isPrivate() const
	{ return isPrivate(m_name); }

	static bool isPrivate(cstr_t &name)
	{ return name[0] != '#'; }

	Containers *getContainers()
	{ return m_containers; }

	// Wrappers to avoid the inclusion of client.h
	void say(cstr_t &text);
	void reply(UserInstance *ui, cstr_t &text);
	void notice(UserInstance *ui, cstr_t &text);
	void leave();

private:
	std::string m_name;
	std::string m_name_settings;

	// Per-channel data
	Containers *m_containers = nullptr;
};


class Network : public IUserOwner {
public:
	Network(IClient *cli) : IUserOwner(cli) {}
	~Network();

	Channel *addChannel(cstr_t &name);
	Channel *getChannel(cstr_t &name) const;
	bool removeChannel(Channel *c);
	bool contains(Channel *c) const;

	std::set<Channel *> &getAllChannels()
	{ return m_channels; }

	//void freeTempChannels();

private:
	//static const time_t TEMP_CHANNEL_TIMEOUT = 300;

	std::string m_last_active_channel;

	std::set<Channel *> m_channels;
	//std::map<Channel *, time_t> m_channels_temp;
};
