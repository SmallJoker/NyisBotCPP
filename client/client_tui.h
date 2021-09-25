#pragma once

#include "client.h"
#include "types.h"
#include <pthread.h>

class ChatCommand;
class IModule;
class Settings;
class TUIhelper;

class ClientTUI : public IClient {
public:
	ClientTUI(Settings *settings);
	~ClientTUI();

	void initialize();
	bool run();

	void sendRaw(cstr_t &text) const;
	void actionSay(Channel *c, cstr_t &text);
	void actionReply(Channel *c, UserInstance *ui, cstr_t &text);
	void actionNotice(Channel *c, UserInstance *ui, cstr_t &text);
	void actionJoin(cstr_t &channel);
	void actionLeave(Channel *c);

protected:
	void processRequest(ClientRequest &cr);

private:
	friend class TUIhelper;

	static void *inputHandler(void *cli_p);

	pthread_t m_thread = 0;
	bool m_is_alive = false;
	IModule *m_helper = nullptr;
	ChatCommand *m_commands = nullptr;
};
