#pragma once

#include "types.h"
#include <thread>

class Connection;
struct ClientActionEntry;

class Client {
public:
	Client(cstr_t &address, int port);
	~Client();

	void send(cstr_t &text);
	bool run();

private:
	void handleUnknown(std::string &msg);
	void handleError(cstr_t &status, std::string &msg);
	void handleClientEvent(cstr_t &status, std::string &msg);
	void handleChatMessage(cstr_t &status, std::string &msg);
	void handleServerMessage(cstr_t &status, std::string &msg);

	Connection *m_con = nullptr;
	static const ClientActionEntry s_actions[];
};
