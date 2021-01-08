#pragma once

#include <string>
#include <unordered_map>

class Channel;

class UserData {
public:
	UserData(const std::string &hostmask) :
		m_hostmask(hostmask) {};
private:
	friend class Channel;
	std::string m_hostmask;
};


class Channel {
public:
	Channel(const std::string &name);

	bool isPrivate() const
	{ return isPrivate(m_name); }

	static bool isPrivate(const std::string &name)
	{ return name[0] != '#'; }

	UserData *getUserData(const std::string &user_name);

	void say(const std::string &what);
	void quit();
private:
	std::string m_name;
	std::unordered_map<std::string, UserData> m_users;
};
