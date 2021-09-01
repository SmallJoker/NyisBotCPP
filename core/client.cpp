#include "client.h"
#include "connection.h"
#include "logger.h"

struct ClientActionEntry {
	int offset;
	const char *status;
	void (Client::*handler)(cstr_t &status, std::string &msg);
};

Client::Client(cstr_t &address, int port)
{
	m_con = new Connection(address, port);
}

Client::~Client()
{
	delete m_con;
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
	if (!m_con)
		return false;

	std::string *what = m_con->popRecv();
	// Process incoming lines, one-by-one
	if (!what)
		return true;

	std::cout << *what << std::endl;

	auto text_pos = what->find(':');
	// TODO: Split message by space until "text_pos" is reached. Put the text into a separate string.


	const ClientActionEntry *action = s_actions;
	while (true) {
		//if (args[action->offset] == action->status) {
		//	break;
		//}

		// Next entry
		action++;
		if (!action->status) {
			handleUnknown(*what);
			return true;
		}
	}

	std::string nick, hostmask;
	if (action->offset == 0) {
		const std::string &arg = ""; //args[0];
		// Format: nickname!hostname
		auto host_pos = arg.find('!');
		if (host_pos != std::string::npos) {
			nick = arg.substr(0, host_pos);
			hostmask = arg.substr(host_pos + 1);
		} else {
			hostmask = arg;
		}
	}

	SLEEP_MS(400);
	return true;
}

void Client::handleUnknown(std::string &msg)
{
	VERBOSE("Unknown: " << msg);
}

void Client::handleError(cstr_t &status, std::string &msg)
{
	
	delete m_con;
	m_con = nullptr;
}

void Client::handleClientEvent(cstr_t &status, std::string &msg)
{
}

void Client::handleChatMessage(cstr_t &status, std::string &msg)
{
}

void Client::handleServerMessage(cstr_t &status, std::string &msg)
{
}

const ClientActionEntry Client::s_actions[] = {
	{ 0, "ERROR", &Client::handleError },
	{ 0, "JOIN", &Client::handleClientEvent },
	{ 0, "NICK", &Client::handleClientEvent },
	{ 0, "PART", &Client::handleClientEvent },
	{ 0, "QUIT", &Client::handleClientEvent },
	{ 1, "PRIVMSG", &Client::handleChatMessage },
	{ 1, "NOTICE",  &Client::handleChatMessage },
	{ 0, "PING", nullptr },
	{ 1, "332", &Client::handleServerMessage },
	{ 1, "353", &Client::handleServerMessage },
	{ 1, "366", &Client::handleServerMessage },
	{ 1, "396", &Client::handleServerMessage },
	{ 1, "004", nullptr },
	{ 1, "005", nullptr },
	{ 1, "252", nullptr },
	{ 1, "254", nullptr },
	{ 1, "265", nullptr },
	{ 1, "266", nullptr },
	{ 1, "333", nullptr },
	{ 0, nullptr, nullptr }, // Termination
};
