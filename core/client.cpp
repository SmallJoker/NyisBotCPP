// Class for the protocol implementation

#include "client.h"
#include "connection.h"
#include "channel.h"
#include "logger.h"
#include "module.h"
#include "settings.h"
#include <cstring>
#include <memory> // unique_ptr

struct ClientActionEntry {
	int offset;
	const char *status;
	void (Client::*handler)(cstr_t &status, NetworkEvent *e);
};

struct NetworkEvent {
	std::string nickname;
	std::string hostmask;
	std::string text;

	std::vector<std::string> args;

	void dump()
	{
		std::cout << "\tPacket dump: ";
		for (const auto &s : args)
			std::cout << s << " ";
		std::cout << ": " << text << std::endl;
	}
};

Client::Client(Settings *settings)
{
	m_settings = settings;

	m_log = new Logger("debug.txt");
	m_network = new Network(this);
	m_module_mgr = new ModuleMgr(this);
}

Client::~Client()
{
	if (m_con && m_auth_status != AS_SEND_NICK) {
		sendRaw("QUIT :Goodbye");
		SLEEP_MS(100);
	}

	delete m_module_mgr;
	m_module_mgr = nullptr;

	delete m_network;
	m_network = nullptr;

	delete m_con;
	m_con = nullptr;

	delete m_log;
	m_log = nullptr;
}

void Client::initialize()
{
	m_nickname = m_settings->get("client.nickname");
	SettingTypeLong at; m_settings->get("client.authtype", &at);
	m_auth_type = at.value;

	const std::string &addr = m_settings->get("client.address");
	SettingTypeLong port;     m_settings->get("client.port", &port);

	m_con = new Connection(addr, port.value);
}

void Client::addTodo(ClientTodo && ct)
{
	m_todo_lock.lock();
	m_todo.push(std::move(ct));
	m_todo_lock.unlock();
}

void Client::processTodos()
{
	if (m_todo.empty())
		return;

	ClientTodo ct;
	{
		m_todo_lock.lock();
		std::swap(m_todo.front(), ct);
		m_todo.pop();
		m_todo_lock.unlock();
	}

	VERBOSE("Got type " << (int)ct.type);
	switch (ct.type) {
		case ClientTodo::CTT_NONE:
			return;
		case ClientTodo::CTT_RELOAD_MODULE:
			m_module_mgr->reloadModule(
				*ct.reload_module.path,
				ct.reload_module.keep_data);
			delete ct.reload_module.path;
			return;
	}
}

void Client::sendRaw(cstr_t &text)
{
	m_con->send(text + '\n');
}

bool Client::run()
{
	if (!m_con) {
		ERROR("Connection died");
		return false;
	}

	std::unique_ptr<std::string> what(m_con->popRecv());
	// Process incoming lines, one-by-one
	if (!what)
		return true;

	*(m_log->get()) << *what << std::endl;

	NetworkEvent e;

	// Extract regular text if available
	auto text_pos = what->find(':');
	{
		if (text_pos == std::string::npos)
			text_pos = what->size();
		else
			e.text = what->substr(text_pos + 1);
	}

	// Split by spaces into "e.args"
	{
		size_t pos = 0;
		while (pos < text_pos) {
			auto space_pos = what->find(' ', pos);
			if (space_pos == std::string::npos)
				space_pos = text_pos;
			if (space_pos > text_pos)
				break;

			// Only add non-empty entries
			if (space_pos > pos)
				e.args.emplace_back(what->substr(pos, space_pos - pos));

			pos = space_pos + 1;
		}
	}

	// Find matching action based on status code and index offset
	const ClientActionEntry *action = s_actions;
	while (true) {
		if (e.args.size() > action->offset) {
			if (e.args[action->offset] == action->status)
				break; // Found match!
		}

		// Try next entry
		action++;
		if (!action->status) {
			// last entry reached
			handleUnknown(*what);
			return true;
		}
	}

	if (action->offset == 1) {
		const std::string &arg = e.args[0];
		// Format: nickname!hostname status ...
		auto host_pos = arg.find('!');
		if (host_pos != std::string::npos) {
			e.nickname = arg.substr(0, host_pos);
			e.hostmask = arg.substr(host_pos + 1);
		} else {
			e.hostmask = arg;
		}
	}

	// Run the action if there is any to run
	if (action->handler)
		(this->*action->handler)(action->status, &e);

	// TODO: This is a bad location
	if (m_auth_status == AS_JOIN_CHANNELS)
		joinChannels();

	return true;
}

void Client::handleUnknown(cstr_t &msg)
{
	VERBOSE("Unknown: " << msg);
}

void Client::handleError(cstr_t &status, NetworkEvent *e)
{
	ERROR(e->text);
	delete m_con;
	m_con = nullptr;
}

void Client::handleClientEvent(cstr_t &status, NetworkEvent *e)
{
	VERBOSE("Client event: " << status);
	e->dump();

	if (status == "JOIN") {
		// nick!host JOIN :channel
		cstr_t &channel = e->text;
		Channel *c = m_network->getChannel(channel);
		if (!c) {
			c = m_network->addChannel(channel);
			c->addUser(m_nickname);
		}

		UserInstance *ui = c->addUser(e->nickname);
		ui->hostmask = e->hostmask;

		if (e->nickname != m_nickname)
			m_module_mgr->onUserJoin(c, ui);
	}
	if (status == "MODE") {
		cstr_t &what = e->args[2];
		cstr_t &modes = e->text;

		if (what[0] == '#') {
			// Channel mode
			return;
		}

		UserInstance *ui = m_network->getUser(what);
		if (!ui) {
			WARN("MODE not implemented for " << what);
			return;
		}

		if (what == m_nickname) {
			// Add and remove user modes
			apply_user_modes(m_user_modes, e->text);

			// Our mode updated. Auth, if not already done.
			if (m_auth_status == AS_AUTHENTICATE) {
				if (m_auth_type > 0)
					sendRaw("PRIVMSG NickServ :identify " + m_settings->get("client.password"));
			}

			if (strchr(m_user_modes, 'r'))
				m_auth_status = AS_JOIN_CHANNELS;
		}
		return;
	}
	if (status == "NICK") {
		// nick!host NICK :NewNickname
		UserInstance *ui = m_network->getUser(e->nickname);
		ui->nickname = e->text;

		m_module_mgr->onUserRename(ui, e->nickname);
		return;
	}
	if (status == "PART" || status == "KICK") {
		// nick!host PART channel :reason
		cstr_t &channel = e->args[2];
		Channel *c = m_network->getChannel(channel);
		UserInstance *ui = c->getUser(e->nickname);
		if (!c || !ui) {
			ERROR("Invalid channel or user");
			return;
		}

		if (e->nickname == m_nickname) {
			m_module_mgr->onChannelLeave(c);
			m_network->removeChannel(c);
			return;
		}

		m_module_mgr->onUserLeave(c, ui);
		c->removeUser(ui);
		return;
	}
	if (status == "QUIT") {
		if (e->nickname == m_nickname) {
			for (Channel *c : m_network->getAllChannels()) {
				m_module_mgr->onChannelLeave(c);
			}
			//delete this; // eeh.. maybe?
			delete m_network;
			m_network = nullptr;
			return;
		}

		// Remove the user from all channels
		UserInstance *ui = m_network->getUser(e->nickname);
		for (Channel *c : m_network->getAllChannels()) {
			if (c->removeUser(ui)) {
				m_module_mgr->onUserLeave(c, ui);
			}
		}
		// .. and from the entire network to drop the instance
		m_network->removeUser(ui);
		ui = nullptr;
		return;
	}
	if (status == "INVITE") {
		sendRaw("JOIN " + e->text);
		return;
	}
}

void Client::handleChatMessage(cstr_t &status, NetworkEvent *e)
{
	// Log this line
	write_timestamp(&std::cout);
	std::cout << " ";
	if (!e->nickname.empty() && e->nickname != m_nickname)
		std::cout << e->nickname << " \t";

	std::cout << e->text << std::endl;

	if (status == "PRIVMSG") {
		// nick!host PRIVMSG where :text text
		if (e->nickname == "NickServ") {
			// Retrieve pending user status requests

			auto args = strsplit(e->text);
			if (m_auth_type == 1) {
				if (args[1] == "ACC") {
					// For IRCd: solanum
					int status = args[2][0] - '0';
					cstr_t &nick = args[0];
				}
			} else if (m_auth_type == 2) {
				if (args[0] == "STATUS") {
					// For IRCd: plexus
					int status = args[2][0] - '0';
					cstr_t &nick = args[1];
				}
			}
			return;
		}

		cstr_t &channel = e->args[2];
		if (channel[0] != '#') {
			WARN("Private message handling is yet not implemented.");
			return;
		}

		Channel *c = m_network->getChannel(channel);
		if (!c) {
			ERROR("Channel " << channel << "does not exist");
			c = m_network->addChannel(channel);
		}

		ICallbackHandler::ChatInfo info;
		info.ui = m_network->getUser(e->nickname);
		info.text = e->text;
		m_module_mgr->onUserSay(c, info);
		return;
	}
}

void Client::handlePing(cstr_t &status, NetworkEvent *e)
{
	//VERBOSE("PONG!");
	sendRaw("PONG " + e->text);
}

void Client::handleAuthentication(cstr_t &status, NetworkEvent *e)
{
	if (m_auth_status == AS_SEND_NICK) {
		LOG("Auth with nick: " << m_nickname);

		sendRaw("USER " + m_nickname + " foo bar :Generic description");
		sendRaw("NICK " + m_nickname);

		SettingTypeLong at; m_settings->get("client.authtype", &at);
		if (at.value > 0)
			m_auth_status = AS_AUTHENTICATE;
		else
			m_auth_status = AS_JOIN_CHANNELS;

		m_network->addUser(m_nickname);
		return;
	}
	if (status == "396") {
		// Hostmask changed. This indicates successful authentication
		m_auth_status = AS_JOIN_CHANNELS;
		return;
	}
}

void Client::handleServerMessage(cstr_t &status, NetworkEvent *e)
{
	VERBOSE("Server message: " << status);
	e->dump();

	int status_i = 0;
	sscanf(status.c_str(), "%i", &status_i);

	if (status_i == 353) {
		// User list
		cstr_t &channel = e->args[4];
		std::vector<std::string> users = strsplit(e->text);

		Channel *c = m_network->addChannel(channel);
		for (auto &name : users) {
			// Trim first character if necessary
			if (strchr("~&@%+", name[0]))
				name = name.substr(1);

			c->addUser(name);
		}
		m_module_mgr->onChannelJoin(c);
		return;
	}
}

void Client::joinChannels()
{
	if (m_auth_status != AS_JOIN_CHANNELS)
		return;
	m_auth_status = AS_DONE;

	auto channels = strsplit(m_settings->get("client.channels"));
	for (const std::string &chan : channels) {
		if (chan.size() < 2 || chan[0] != '#')
			continue;

		sendRaw("JOIN " + chan);
	}
}

const ClientActionEntry Client::s_actions[] = {
	{ 0, "ERROR", &Client::handleError },
// Init messages and auth
	{ 1, "001", &Client::handleAuthentication },
	{ 1, "002", &Client::handleAuthentication },
	{ 1, "003", &Client::handleAuthentication },
	{ 1, "396", &Client::handleAuthentication }, // Hostmask changed
	{ 1, "439", &Client::handleAuthentication },
	{ 1, "451", &Client::handleAuthentication }, // ERR_NOTREGISTERED
// Server information and events
	{ 0, "PING", &Client::handlePing },
	{ 1, "251", &Client::handleChatMessage }, // RPL_LUSERCLIENT
	{ 1, "253", &Client::handleChatMessage }, // RPL_LUSERUNKNOWN
	{ 1, "255", &Client::handleChatMessage }, // RPL_LUSERME
	{ 1, "332", &Client::handleChatMessage }, // RPL_TOPIC
	{ 1, "372", &Client::handleChatMessage }, // RPL_MOTD
	{ 1, "375", &Client::handleChatMessage }, // RPL_MOTDSTART
	{ 1, "376", &Client::handleChatMessage }, // RPL_ENDOFMOTD
	{ 1, "353", &Client::handleServerMessage },  // RPL_NAMREPLY (user list)
	{ 1, "366", &Client::handleChatMessage },  // RPL_ENDOFNAMES
// Client/user events
	{ 1, "JOIN", &Client::handleClientEvent },
	{ 1, "NICK", &Client::handleClientEvent },
	{ 1, "KICK", &Client::handleClientEvent },
	{ 1, "PART", &Client::handleClientEvent },
	{ 0, "QUIT", &Client::handleClientEvent },
	{ 1, "MODE",  &Client::handleClientEvent },
	{ 1, "PRIVMSG", &Client::handleChatMessage },
	{ 1, "NOTICE",  &Client::handleChatMessage },
	{ 1, "INVITE", &Client::handleClientEvent },
// Ignore
	{ 1, "004", nullptr },
	{ 1, "005", nullptr },
	{ 1, "252", nullptr },
	{ 1, "254", nullptr },
	{ 1, "265", nullptr },
	{ 1, "266", nullptr },
	{ 1, "333", nullptr },
	{ 0, nullptr, nullptr }, // Termination
};
