#pragma once

#include "container.h"
#include "types.h"
#include "utils.h"
#include <set>

class Channel;
class IClient;
class IUserOwner;

// Base class to keep Client-specific data

struct IImplId {
	DISABLE_COPY(IImplId)

	virtual ~IImplId() = default;
	// Manual copy
	virtual IImplId *copy(void *parent) const = 0;

	// ID comparison
	virtual bool is(const IImplId *other) const = 0;

	// Converts the ID to a string representation
	virtual std::string idStr() const = 0;

	// Readable assigned name
	virtual std::string nameStr() const = 0;

protected:
	IImplId() = default;
};


// This is network-wide

class UserInstance : public Containers {
public:
	UserInstance(const IImplId &uid);
	~UserInstance() { delete uid; }

	int getRefs() const { return m_references; }

	IImplId const *uid;

	std::string nickname;
	bool is_bot = false;

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

	int m_references = 0;
};

class IUserOwner {
public:
	IUserOwner(IClient *cli);
	virtual ~IUserOwner();

	UserInstance *addUser(const IImplId &uid);
	UserInstance *getUser(const IImplId &uid) const;
	UserInstance *getUser(cstr_t &name) const;
	bool removeUser(UserInstance *ui);
	bool contains(UserInstance *ui) const;

	IFormatter *createFormatter() const;

	const std::set<UserInstance *> &getAllUsers() const
	{ return m_users; }

protected:
	IClient *m_client;

	// Per-user data
	std::set<UserInstance *> m_users;
};

class Channel : public IUserOwner {
public:
	Channel(bool is_private, const IImplId &cid, IClient *cli);
	~Channel();

	IImplId const *cid;

	// Settings-safe name
	cstr_t &getSettingsName() const
	{ return m_name_settings; }

	bool isPrivate() const
	{ return m_is_private; }

	Containers *getContainers()
	{ return m_containers; }

	// Wrappers to avoid the inclusion of client.h
	void say(cstr_t &text);
	void reply(UserInstance *ui, cstr_t &text);
	void notice(UserInstance *ui, cstr_t &text);
	void leave();

private:
	bool m_is_private;
	std::string m_name_settings;

	// Per-channel data
	Containers *m_containers = nullptr;
};


class Network : public IUserOwner {
public:
	Network(IClient *cli) : IUserOwner(cli) {}
	~Network();

	Channel *addChannel(bool is_private, const IImplId &cid);
	Channel *getChannel(const IImplId &cid) const;
	bool removeChannel(Channel *c);
	bool contains(Channel *c) const;

	std::set<Channel *> &getAllChannels()
	{ return m_channels; }

	//void freeTempChannels();

private:
	//static const time_t TEMP_CHANNEL_TIMEOUT = 300;

	std::set<Channel *> m_channels;
	//std::map<Channel *, time_t> m_channels_temp;
};
