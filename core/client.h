#pragma once

#include "types.h"
#include <thread>

class Connection;
class Settings;
struct ClientActionEntry;
struct NetworkEvent;

class Client {
public:
	Client(Settings *settings);
	~Client();

	void send(cstr_t &text);
	bool run();

private:
	void handleUnknown(cstr_t &msg);
	void handleError(cstr_t &status, NetworkEvent *e);
	void handleClientEvent(cstr_t &status, NetworkEvent *e);
	void handleChatMessage(cstr_t &status, NetworkEvent *e);
	void handlePing(cstr_t &status, NetworkEvent *e);
	void handleAuthentication(cstr_t &status, NetworkEvent *e);
	void handleServerMessage(cstr_t &status, NetworkEvent *e);

	void onReady();

	Settings *m_settings = nullptr;
	Connection *m_con = nullptr;
	static const ClientActionEntry s_actions[];

	bool m_auth_sent = false;
	bool m_ready_called = false;
};
