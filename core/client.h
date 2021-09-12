#pragma once

#include "utils.h"
#include <thread>

class Connection;
class ModuleMgr;
class Network;
class Settings;
struct ClientActionEntry;
struct NetworkEvent;

class Client {
public:
	Client(Settings *settings);
	~Client();

	Settings *getSettings() const
	{ return m_settings; }
	ModuleMgr *getModuleMgr() const
	{ return m_module_mgr; }
	Network *getNetwork() const
	{ return m_network; }

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

	Connection *m_con = nullptr;
	ModuleMgr *m_module_mgr = nullptr;
	Network *m_network = nullptr;
	Settings *m_settings = nullptr;
	static const ClientActionEntry s_actions[];

	bool m_auth_sent = false;
	bool m_ready_called = false;
};
