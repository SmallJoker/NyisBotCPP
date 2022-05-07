#pragma once

#include "client.h"
#include "types.h"
#include <picojson.h>
#include <map>

class IModule;
class Settings;
struct ClientTelegramActionEntry;
struct ClientTelegramUserData;
struct IImplId;

class ClientTelegram : public IClient {
public:
	ClientTelegram(Settings *settings);
	~ClientTelegram();

	void initialize();
	bool run();

	// Module functions: Actions
	void sendRaw(cstr_t &text) {}
	void actionSay(Channel *c, cstr_t &text);
	void actionReply(Channel *c, UserInstance *ui, cstr_t &text);
	void actionNotice(Channel *c, UserInstance *ui, cstr_t &text);
	void actionJoin(cstr_t &channel);
	void actionLeave(Channel *c);

	// Module functions: Utilities
	IFormatter *createFormatter() const;

protected:
	void processRequest(ClientRequest &cr) {}

private:
	// Creates a new HTTP request
	// post_json: managed by caller
	// return:    managed by caller
	picojson::value *requestREST(cstr_t &method, cstr_t &url, picojson::object *post_json = nullptr);
	Channel *joinChannelIfNeeded(bool is_private, IImplId *cid);
	ClientTelegramUserData *getUserDataOrCreate(UserInstance *ui);

	static const ClientTelegramActionEntry s_actions[];

	void processUpdate(picojson::value &update);
	void handleUnknown(picojson::value &v);
	void handleMessage(cstr_t &type, picojson::value &v);

	std::chrono::high_resolution_clock::time_point m_last_step;
	time_t m_start_time;
	int64_t m_my_user_id = 0;
	int64_t m_last_update_id = 0;
	std::string m_request_url;
};
