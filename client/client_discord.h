#pragma once

#include "client.h"
#include "types.h"

class IModule;
class Settings;

enum DiscordStatus {
	DS_IDLE,
	DS_CONNECT,
	DS_OBTAIN_TOKEN,
	DS_STATUSINFO,
};

class ClientDiscord : public IClient {
public:
	ClientDiscord(Settings *settings);
	~ClientDiscord();

	void initialize();
	bool run();

	void sendRaw(cstr_t &text) const {}
	void actionSay(Channel *c, cstr_t &text) {}
	void actionReply(Channel *c, UserInstance *ui, cstr_t &text) {}
	void actionNotice(Channel *c, UserInstance *ui, cstr_t &text) {}
	void actionJoin(cstr_t &channel) {}
	void actionLeave(Channel *c) {}

protected:
	void processRequest(ClientRequest &cr) {}
	// Creates a new HTTP request
	// post_json: picojson::object (managed by caller)
	// return:    picojson::value  (managed by caller)
	void *requestREST(cstr_t &method, cstr_t &url, void *post_json = nullptr);

private:
	DiscordStatus m_status = DS_IDLE;

	std::string m_token;
};
