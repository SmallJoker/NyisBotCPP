#include "channel.h"
#include "client.h"
#include "logger.h"
#include "settings.h"


// ================= UserInstance =================

UserInstance::UserInstance(const IImplId &uid)
{
	this->nickname = "(nickname)";
	this->uid = uid.copy(this);
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

UserInstance *IUserOwner::addUser(const IImplId &uid)
{
	UserInstance *ui = getUser(uid);

	if (!ui) {
		// Network must always have a reference
		if (this == m_client->getNetwork())
			ui = new UserInstance(uid);
		else
			ui = m_client->getNetwork()->addUser(uid);
		ui->grabRef();
	}

	// std::set has unique keys
	m_users.insert(ui);
	return ui;
}

UserInstance *IUserOwner::getUser(const IImplId &uid) const
{
	for (UserInstance *ui : m_users) {
		if (uid.is(ui->uid))
			return ui;
	}
	return nullptr;
}

UserInstance *IUserOwner::getUser(cstr_t &name) const
{
	return m_client->findUser(this, name);
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

bool IUserOwner::contains(UserInstance *ui) const
{
	return m_users.find(ui) != m_users.end();
}

IFormatter *IUserOwner::createFormatter() const
{
	return m_client->createFormatter();
}


// ================= Channel =================

Channel::Channel(bool is_private, const IImplId &cid, IClient *cli) :
	IUserOwner(cli), cid(cid.copy(this))
{
	m_is_private = is_private;
	m_containers = new Containers();

	if (is_private) {
		// Error indicator
		m_name_settings = "PRIVATE_" + this->cid->nameStr();
	} else {
		m_name_settings = this->cid->idStr();
		if (m_name_settings[0] == '#') // IRC compat
			m_name_settings = m_name_settings.substr(1);
	}
}

Channel::~Channel()
{
	delete m_containers;
	m_containers = nullptr;

	for (UserInstance *ui : m_users)
		ui->dropRef();
	m_users.clear();

	delete cid;
	cid = nullptr;
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
	for (Channel *c : m_channels)
		delete c;
	m_channels.clear();

	for (UserInstance *ui : m_users)
		delete ui;
	m_users.clear();
}

Channel *Network::addChannel(bool is_private, const IImplId &cid)
{
	Channel *c = getChannel(cid);
	if (!c) {
		c = new Channel(is_private, cid, m_client);
		m_channels.insert(c);
	}
	return c;
}

Channel *Network::getChannel(const IImplId &cid) const
{
	for (Channel *it : m_channels) {
		if (it->cid->is(&cid))
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
