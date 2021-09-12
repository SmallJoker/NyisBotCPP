// Class for the protocol implementation

#include "client.h"
#include "connection.h"
#include "channel.h"
#include "logger.h"
#include "module.h"
#include "settings.h"
#include <ctime>
#include <cstring>
#include <iomanip> // std::put_time

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
		std::cout << "Packet dump: ";
		for (const auto &s : args)
			std::cout << s << " ";
		std::cout << ": " << text << std::endl;
	}
};

Client::Client(Settings *settings)
{
	m_settings = settings;

	m_network = new Network(this);
	m_module_mgr = new ModuleMgr(this);

	const std::string &addr = settings->get("client.address");
	SettingTypeLong port;     settings->get("client.port", &port);

	m_con = new Connection(addr, port.value);
}

Client::~Client()
{
	send("QUIT :Goodbye");
	SLEEP_MS(100);

	delete m_module_mgr;
	m_module_mgr = nullptr;

	delete m_network;
	m_network = nullptr;

	delete m_con;
	m_con = nullptr;
}


void Client::send(cstr_t &text)
{
	m_con->send(text + '\n');
}

static std::string get_next_part(std::string &input)
{
	auto pos = input.find(' ');
	std::string value;
	if (pos == std::string::npos) {
		std::swap(value, input);
		return value;
	}

	value = input.substr(0, pos);
	input = input.substr(pos + 1);
	return value;
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

	//VERBOSE(*what);
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
		cstr_t channel = e->text.substr(e->text[0] == ':');
		Channel *c = m_network->getOrCreateChannel(channel);

		UserInstance *ui = c->addUser(e->nickname);
		ui->hostmask = e->hostmask;

		m_module_mgr->onUserJoin(ui);
	}
	if (status == "PART") {
		cstr_t &channel = e->args[2];
		Channel *c = m_network->getOrCreateChannel(channel);

		UserInstance *ui = c->getUser(e->nickname);
		if (!ui) {
			ERROR("Unknown user: " << e->nickname);
			return;
		}

		m_module_mgr->onUserLeave(ui);
		c->removeUser(ui);
		return;
	}
}

void Client::handleChatMessage(cstr_t &status, NetworkEvent *e)
{
	// Log this line
	std::time_t time = std::time(nullptr);
	std::tm *tm = std::localtime(&time);
	std::cout << std::put_time(tm, "[%T]") << " ";
	if (!e->nickname.empty() && e->nickname != m_settings->get("client.nickname"))
		std::cout << e->nickname << ": ";

	std::cout << e->text << std::endl;

	if (status == "376")
		onReady();
}

void Client::handlePing(cstr_t &status, NetworkEvent *e)
{
	VERBOSE("PONG!");
	send("PONG " + e->text);
}

void Client::handleAuthentication(cstr_t &status, NetworkEvent *e)
{
	if (m_auth_sent)
		return;

	m_auth_sent = true;

	const std::string &nick = m_settings->get("client.nickname");
	LOG("Auth with nick: " << nick);

	send("USER " + nick + " foo bar :Generic description");
	send("NICK " + nick);
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
		std::vector<std::string> users = strsplit(e->text, ' ');

		Channel *c = m_network->getOrCreateChannel(channel);
		for (auto &name : users) {
			// Trim first character if necessary
			if (strchr("~&@%+", name[0]))
				name = name.substr(1);

			UserInstance *ui = c->addUser(name);
			m_module_mgr->onUserJoin(ui);
		}
		return;
	}
}

void Client::onReady()
{
	if (m_ready_called)
		return;
	m_ready_called = true;

	auto channels = strsplit(m_settings->get("client.channels"), ' ');
	for (const std::string &chan : channels) {
		if (chan.size() < 2 || chan[0] != '#')
			continue;
		send("JOIN " + chan);
	}
}

const ClientActionEntry Client::s_actions[] = {
	{ 0, "ERROR", &Client::handleError },
// Init messages and auth
	{ 1, "001", &Client::handleAuthentication },
	{ 1, "002", &Client::handleAuthentication },
	{ 1, "003", &Client::handleAuthentication },
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
	{ 1, "396", &Client::handleServerMessage },
// Client/user events
	{ 1, "JOIN", &Client::handleClientEvent },
	{ 0, "NICK", &Client::handleClientEvent },
	{ 0, "PART", &Client::handleClientEvent },
	{ 0, "QUIT", &Client::handleClientEvent },
	{ 1, "MODE", nullptr }, // stub
	{ 1, "PRIVMSG", &Client::handleChatMessage },
	{ 1, "NOTICE",  &Client::handleChatMessage },
// Ignore
	{ 1, "004", nullptr },
	{ 1, "004", nullptr },
	{ 1, "004", nullptr },
	{ 1, "004", nullptr },
	{ 1, "005", nullptr },
	{ 1, "252", nullptr },
	{ 1, "254", nullptr },
	{ 1, "265", nullptr },
	{ 1, "266", nullptr },
	{ 1, "333", nullptr },
	{ 0, nullptr, nullptr }, // Termination
};
