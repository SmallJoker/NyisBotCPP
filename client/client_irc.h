#pragma once

#include "client.h"
#include "types.h"

class Connection;
class Logger;
class ModuleMgr;
class Network;
class Settings;
struct ClientActionEntry;
struct NetworkEvent;

class ClientIRC : public IClient {
public:
	ClientIRC(Settings *settings);
	~ClientIRC();

	void initialize();
	bool run();

	void sendRaw(cstr_t &text) const;
	// Functions for Channel
	void actionSay(Channel *c, cstr_t &text);
	void actionReply(Channel *c, UserInstance *ui, cstr_t &text);
	void actionNotice(Channel *c, UserInstance *ui, cstr_t &text);
	void actionJoin(cstr_t &channel);
	void actionLeave(Channel *c);

protected:
	void processRequest(ClientRequest &cr);

private:
	void handleUnknown(cstr_t &msg);
	void handleError(cstr_t &status, NetworkEvent *e);
	void handleClientEvent(cstr_t &status, NetworkEvent *e);
	void handleChatMessage(cstr_t &status, NetworkEvent *e);
	void handlePing(cstr_t &status, NetworkEvent *e);
	void handleAuthentication(cstr_t &status, NetworkEvent *e);
	void handleServerMessage(cstr_t &status, NetworkEvent *e);

	void joinChannels();
	void requestAccStatus(UserInstance *ui) const;

	Connection *m_con = nullptr;
	static const ClientActionEntry s_actions[];

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
