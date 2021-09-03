#pragma once

#include "types.h"
#include <thread>

class Connection;
struct ClientActionEntry;
struct NetworkEvent;

class Client {
public:
	Client(cstr_t &address, int port);
	~Client();

	void send(cstr_t &text);
	bool run();

private:
	void handleUnknown(cstr_t &msg);
	void handleError(cstr_t &status, NetworkEvent *e);
	void handleClientEvent(cstr_t &status, NetworkEvent *e);
	void handleChatMessage(cstr_t &status, NetworkEvent *e);
	void handlePing(cstr_t &status, NetworkEvent *e);
	void handleServerMessage(cstr_t &status, NetworkEvent *e);

	Connection *m_con = nullptr;
	static const ClientActionEntry s_actions[];
};
