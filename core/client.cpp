#include "client.h"
#include "connection.h"
#include "logger.h"
#include <ctime>
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
	if (!m_con) {
		ERROR("Connection died");
		return false;
	}

	std::unique_ptr<std::string> what(m_con->popRecv());
	// Process incoming lines, one-by-one
	if (!what)
		return true;

	NetworkEvent e;

	// Extract regular text if available
	auto text_pos = what->find(':');
	{
		if (text_pos == std::string::npos)
			text_pos = what->size();

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
	std::cout << "\t READ: ";
	for (const auto &s : e.args)
		std::cout << s << ", ";
	std::cout << "; " << e.text << ";" << std::endl;

	const ClientActionEntry *action = s_actions;
	while (true) {
		if (e.args.size() <= action->offset)
			continue;

		if (e.args[action->offset] == action->status)
			break; // Found match!

		// Try next entry
		action++;
		if (!action->status) {
			// last entry reached
			handleUnknown(*what);
			return true;
		}
	}

	if (action->offset == 0) {
		const std::string &arg = e.args[0];
		// Format: nickname!hostname
		auto host_pos = arg.find('!');
		if (host_pos != std::string::npos) {
			e.nickname = arg.substr(0, host_pos);
			e.hostmask = arg.substr(host_pos + 1);
		} else {
			e.hostmask = arg;
		}
	}

	if (action->handler)
		(this->*action->handler)(action->status, &e);

	SLEEP_MS(400);
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
	VERBOSE("Client event " << status);
}

void Client::handleChatMessage(cstr_t &status, NetworkEvent *e)
{
	std::time_t time = std::time(nullptr);
	std::tm *tm = std::localtime(&time);
	std::cout << std::put_time(tm, "[%T]") << e->nickname << ": " << e->text << std::endl;
}

void Client::handlePing(cstr_t &status, NetworkEvent *e)
{
	VERBOSE("PONG!");
	send("PONG " + e->text);
}

void Client::handleServerMessage(cstr_t &status, NetworkEvent *e)
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
	{ 0, "PING", &Client::handlePing },
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
