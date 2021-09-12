#include "channel.h"
#include "logger.h"
#include "utils.h"

Channel::Channel(cstr_t &name)
{
	if (isPrivate(name)) {
		WARN("Attempt to add private channel " << name);
		return;
	}

	m_containers = new Containers();
}

Channel::~Channel()
{
	delete m_containers;
	m_containers = nullptr;

	m_users.clear();
}

UserInstance *Channel::addUser(cstr_t &name)
{
	UserInstance *ui = getUser(name);
	if (!ui) {
		ui = new UserInstance(name);
		m_users.insert(ui);
	}
	return ui;
}

UserInstance *Channel::getUser(cstr_t &name) const
{
	for (auto &ui : m_users) {
		if (ui->nickname == name)
			return ui;
	}
	return nullptr;
}


bool Channel::removeUser(UserInstance *ui)
{
	auto it = m_users.find(ui);
	if (it == m_users.end())
		return false;

	delete *it;
	m_users.erase(it);
	return true;
}


void Channel::say(cstr_t &what)
{
	WARN("stub");
}

void Channel::quit()
{
	WARN("stub");
}

Channel *Network::getChannel()
{
	for (Channel *it : m_channels) {
		if (it->getName() == m_last_active_channel)
			return it;
	}
	return nullptr;
}

Channel *Network::getOrCreateChannel(cstr_t &name)
{
	for (Channel *it : m_channels) {
		if (it->getName() == name)
			return it;
	}

	Channel *c = new Channel(name);
	m_channels.insert(c);
	return c;
}

