#include "inc/channel.h"
#include "inc/logger.h"


UserData *Channel::getUserData(const std::string &user_name)
{
	auto it = m_users.find(user_name);
	if (it == m_users.end())
		return nullptr;

	return &(it->second);
}

void Channel::say(const std::string &what)
{
	WARN("stub");
}

void Channel::quit()
{
	WARN("stub");
}
