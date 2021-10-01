#include "channel.h"
#include "client.h"
#include "logger.h"
#include "settings.h"


// ================= UserInstance =================

UserInstance::UserInstance(cstr_t &name)
{
	nickname = name;
	m_references = 0;
}


// ================= IUserOwner =================

IUserOwner::IUserOwner(IClient *cli)
{
	if (!cli)
		ERROR("Client must not be NULL");

	m_client = cli;
}

IUserOwner::~IUserOwner()
{
	for (UserInstance *ui : m_users)
		ui->dropRef();

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
		ui->grabRef();
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

	ui->dropRef();
	m_users.erase(it);
	return true;
}


// ================= Channel =================

Channel::Channel(cstr_t &name, IClient *cli) :
	IUserOwner(cli)
{
	m_name = name;
	m_client = cli;
	m_containers = new Containers();

	if (!isPrivate(name)) {
		m_name_settings = name.substr(name[0] == '#');
		Settings::sanitizeKey(m_name_settings);
	}
}

Channel::~Channel()
{
	delete m_containers;
	m_containers = nullptr;

	for (UserInstance *ui : m_users)
		ui->dropRef();
	m_users.clear();
}

bool Channel::contains(UserInstance *ui) const
{
	return m_users.find(ui) != m_users.end();
}

void Channel::say(cstr_t &text)
{
	m_client->actionSay(this, text);
}

void Channel::reply(UserInstance *ui, cstr_t &text)
{
	m_client->actionReply(this, ui, text);
}

void Channel::notice(UserInstance *ui, cstr_t &text)
{
	m_client->actionNotice(this, ui, text);
}

void Channel::leave()
{
	m_client->actionLeave(this);
}


// ================= Network =================

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

bool Network::contains(Channel *c) const
{
	return m_channels.find(c) != m_channels.end();
}
