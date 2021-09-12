#include "channel.h"
#include "client.h"
#include "logger.h"
#include "utils.h"

IUserOwner::IUserOwner(Client *cli)
{
	if (!cli)
		ERROR("Client must not be NULL");

	m_client = cli;
}

IUserOwner::~IUserOwner()
{
	for (UserInstance *ui : m_users)
		ui->drop();

	m_users.clear();
}

UserInstance *IUserOwner::addUser(cstr_t &name)
{
	UserInstance *ui = getUser(name);

	if (!ui) {
		// Network must always have a reference
		if (this == m_client->getNetwork())
			ui = new UserInstance(name);
		else
			ui = m_client->getNetwork()->addUser(name);
		ui->grab();
	}

	// std::set has unique keys
	m_users.insert(ui);
	return ui;
}

UserInstance *IUserOwner::getUser(cstr_t &name) const
{
	for (UserInstance *ui : m_users) {
		if (ui->nickname == name)
			return ui;
	}
	return nullptr;
}


bool IUserOwner::removeUser(UserInstance *ui)
{
	auto it = m_users.find(ui);
	if (it == m_users.end())
		return false;

	// Remove from all channels before network itself
	Network *net = m_client->getNetwork();
	if (this == net) {
		for (Channel *c : net->getAllChannels())
			c->removeUser(ui);
	}

	ui->drop();
	m_users.erase(it);
	return true;
}


Channel::Channel(cstr_t &name, Client *cli) :
	IUserOwner(cli)
{
	if (isPrivate(name))
		WARN("Attempt to add private channel " << name);

	m_name = name;
	m_client = cli;
	m_containers = new Containers();
}

Channel::~Channel()
{
	delete m_containers;
	m_containers = nullptr;

	for (UserInstance *ui : m_users)
		ui->drop();
	m_users.clear();
}

void Channel::say(cstr_t &text)
{
	m_client->send("PRIVMSG " + m_name + " :" + text);
}

void Channel::leave()
{
	m_client->send("PART " + m_name);
}


Network::~Network()
{
	for (UserInstance *ui : m_users)
		delete ui;
	m_users.clear();

	for (Channel *c : m_channels)
		delete c;
	m_channels.clear();
}

Channel *Network::addChannel(cstr_t &name)
{
	for (Channel *it : m_channels) {
		if (it->getName() == name)
			return it;
	}

	Channel *c = new Channel(name, m_client);
	m_channels.insert(c);
	return c;
}

Channel *Network::getChannel(cstr_t &name) const
{
	for (Channel *it : m_channels) {
		if (it->getName() == name)
			return it;
	}
	return nullptr;
}

Channel *Network::getChannel() const
{
	for (Channel *it : m_channels) {
		if (it->getName() == m_last_active_channel)
			return it;
	}
	return nullptr;
}

bool Network::removeChannel(Channel *c)
{
	auto it = m_channels.find(c);
	if (it == m_channels.end())
		return false;

	std::set<UserInstance *> ui_copy = c->getAllUsers();

	delete c;
	m_channels.erase(it);

	for (UserInstance *ui : ui_copy) {
		if (ui->getRefs() <= 1) {
			// Clean up garbage left over after channel removal
			removeUser(ui);
		}
	}
	return true;
}

