#pragma once

#include "container.h"
#include "types.h"
#include <queue>

class Connection;
class Logger;
class ModuleMgr;
class Network;
class Settings;
struct ClientActionEntry;
struct ClientTodo;
struct NetworkEvent;

class Client {
public:
	Client(Settings *settings);
	~Client();

	void initialize();

	Settings *getSettings() const
	{ return m_settings; }
	ModuleMgr *getModuleMgr() const
	{ return m_module_mgr; }
	Network *getNetwork() const
	{ return m_network; }

	void addTodo(ClientTodo && ct);
	void processTodos();

	void sendRaw(cstr_t &text);
	bool run();

private:
	void handleUnknown(cstr_t &msg);
	void handleError(cstr_t &status, NetworkEvent *e);
	void handleClientEvent(cstr_t &status, NetworkEvent *e);
	void handleChatMessage(cstr_t &status, NetworkEvent *e);
	void handlePing(cstr_t &status, NetworkEvent *e);
	void handleAuthentication(cstr_t &status, NetworkEvent *e);
	void handleServerMessage(cstr_t &status, NetworkEvent *e);

	void joinChannels();

	Connection *m_con = nullptr;
	Logger *m_log = nullptr;
	ModuleMgr *m_module_mgr = nullptr;
	Network *m_network = nullptr;
	Settings *m_settings = nullptr;
	static const ClientActionEntry s_actions[];

	std::mutex m_todo_lock;
	std::queue<ClientTodo> m_todo;

	enum AuthStatus {
		AS_SEND_NICK,
		AS_AUTHENTICATE,
		AS_JOIN_CHANNELS,
		AS_DONE
	};
	AuthStatus m_auth_status = AS_SEND_NICK;

	// Cached settings
	std::string m_nickname;
	long m_auth_type;

	// User mode. This should be enough space.
	char m_user_modes[13] = "            ";
};


struct ChannelUserContainer : public IContainer {
	enum ChannelPermissions {
		CP_NONE,
		CP_CASUAL,
		CP_IDENTIFIED,
		CP_ADMIN
	};
	std::string modes;
	ChannelPermissions perm;
};

struct ClientTodo {
	enum ClientTodoType {
		CTT_NONE,
		CTT_RELOAD_MODULE,
	} type = CTT_NONE;

	union {
		struct {
			std::string *path;
			bool keep_data;
		} reload_module;
	};
};
