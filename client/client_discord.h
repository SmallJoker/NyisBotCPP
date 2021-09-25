#pragma once

#include "client.h"
#include "types.h"

class ChatCommand;
class IModule;
class Settings;

class ClientDiscord : public IClient {
public:
	ClientDiscord(Settings *settings);
	~ClientDiscord() {}

	void initialize() {}
	bool run() { return false; }

	void sendRaw(cstr_t &text) const {}
	void actionSay(Channel *c, cstr_t &text) {}
	void actionReply(Channel *c, UserInstance *ui, cstr_t &text) {}
	void actionNotice(Channel *c, UserInstance *ui, cstr_t &text) {}
	void actionJoin(cstr_t &channel) {}
	void actionLeave(Channel *c) {}

protected:
	void processRequest(ClientRequest &cr) {}

private:
	ChatCommand *m_commands = nullptr;
};
