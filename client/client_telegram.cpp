#include "client_telegram.h"
#include "channel.h"
#include "connection.h"
#include "logger.h"
#include "module.h"
#include "settings.h"
#include <memory>
#include <sstream>


// ============ User ID ============

struct UserIdTelegram : IImplId {
	UserIdTelegram(int64_t id) : user_id(id) {}
	IImplId *copy(void *parent) const { return new UserIdTelegram(user_id); }

	bool is(const IImplId *other) const
	{
		return user_id == ((UserIdTelegram *)other)->user_id;
	}

	std::string str() const
	{
		return std::to_string(user_id);
	}

	int64_t user_id;
};


// ============ Formatter ============

class FormatterTelegram : public IFormatter {
public:
	void mention(UserInstance *ui)
	{
		*m_os << "[" << ui->nickname << "](tg://user?id="
			<< ui->uid->str() << ")";
	}

	void beginImpl(IRC_Color color)
	{
		// requires HTML style
	}

	void beginImpl(FormatType flags)
	{
		if (flags & FT_BOLD)
			*m_os << "**";
		if (flags & FT_ITALICS)
			*m_os << "_";
		if (flags & FT_UNDERL)
			*m_os << "__";
	}

	void endImpl(FormatType flags)
	{
		if (flags & FT_BOLD)
			*m_os << "**";
		if (flags & FT_ITALICS)
			*m_os << "_";
		if (flags & FT_UNDERL)
			*m_os << "_\r_";
		//if (flags & FT_COLOR)
	}
};


// ============ Client class ============

struct ClientTelegramActionEntry {
	const char *type;
	void (ClientTelegram::*handler)(cstr_t &type, picojson::value &v);
};

ClientTelegram::ClientTelegram(Settings *settings) :
	IClient(settings)
{
	m_request_url = "https://api.telegram.org/bot" + settings->get("telegram.token") + "/";
	SettingType::parseS64(m_settings->get("telegram.cache_update_id"), &m_last_update_id);

	m_last_step = std::chrono::high_resolution_clock::now();
	m_start_time = time(nullptr);
}

ClientTelegram::~ClientTelegram()
{
	m_settings->syncFileContents(SR_WRITE);
}

#define REQUEST_WRAP(var, arg1, arg2, arg3) \
	std::unique_ptr<picojson::value> var(requestREST(arg1, arg2, arg3))

void ClientTelegram::initialize()
{
	REQUEST_WRAP(out_me, "GET", "getMe", nullptr);
	if (out_me) {
		m_my_user_id = out_me->get("id").get<int64_t>();
	}
}

void ClientTelegram::actionSay(Channel *c, cstr_t &text)
{
	if (text.empty())
		return;

	picojson::object inp;
	inp["chat_id"] = picojson::value(c->getName());
	inp["text"] = picojson::value(text);

	REQUEST_WRAP(out, "POST", "sendMessage", &inp);
}

void ClientTelegram::actionReply(Channel *c, UserInstance *ui, cstr_t &text)
{
	std::unique_ptr<IFormatter> fmt(createFormatter());

	fmt->mention(ui);
	*fmt << ": `" << text << "`";

	picojson::object inp;
	inp["chat_id"] = picojson::value(c->getName());
	inp["parse_mode"] = picojson::value("MarkdownV2");
	inp["text"] = picojson::value(fmt->str());

	REQUEST_WRAP(out, "POST", "sendMessage", &inp);
}

void ClientTelegram::actionNotice(Channel *c, UserInstance *ui, cstr_t &text)
{
	Channel *ui_c = joinChannelIfNeeded(ui->nickname);
	actionSay(ui_c, "NOTICE : " + text);
}

void ClientTelegram::actionJoin(cstr_t &channel)
{
	WARN("Cannot join channel: " << channel);
}

void ClientTelegram::actionLeave(Channel *c)
{
	picojson::object inp;
	inp["chat_id"] = picojson::value(c->getName());

	REQUEST_WRAP(out, "POST", "leaveChat", &inp);
	if (out)
		LOG(out->serialize(true));
}

IFormatter *ClientTelegram::createFormatter() const
{
	return new FormatterTelegram();
}

bool ClientTelegram::run()
{
	m_module_mgr->onStep(-1);

	auto time_now = std::chrono::high_resolution_clock::now();
	if (std::chrono::duration<float>(time_now - m_last_step).count() < 0.5f)
		return true;
	m_last_step = time_now;

	// Polling
	picojson::object inp;
	inp["offset"] = picojson::value(m_last_update_id + 1);

	REQUEST_WRAP(out, "GET", "getUpdates", &inp);
	if (!out)
		return true;

	int64_t highest_update_id = m_last_update_id;
	for (auto &v : out->get<picojson::array>()) {
		int64_t id = v.get("update_id").get<int64_t>();
		if (id <= m_last_update_id)
			continue;

		highest_update_id = std::max(highest_update_id, id);

		// Handle events
		processUpdate(v);
	}

	m_last_update_id = highest_update_id;
	m_settings->set("telegram.cache_update_id", std::to_string(m_last_update_id));

	return true;
}

void ClientTelegram::processUpdate(picojson::value &update)
{
	// Find matching action based on status code and index offset
	const ClientTelegramActionEntry *action = s_actions;
	picojson::value *v = nullptr;
	for (auto *action = s_actions; action->handler; ++action) {
		v = &update.get(action->type);
		if (v->is<picojson::object>()) {
			// Good!
			break;
		}
	}

	if (!action->handler) {
		handleUnknown(update);
		return;
	}

	try {
		(this->*action->handler)(action->type, *v);
	} catch (std::runtime_error &e) {
		ERROR(e.what() << "\n" << v->serialize(false));
	}
}

picojson::value *ClientTelegram::requestREST(cstr_t &method, cstr_t &url, picojson::object *post_json)
{
	// Testing: https://jsonplaceholder.typicode.com/guide/

	std::unique_ptr<Connection> con(Connection::createHTTP(method, m_request_url + url));
	// Connection: Send
	if (post_json)
		con->addHTTP_Header("Content-Type: application/json; charset=UTF-8");
	else
		con->addHTTP_Header("Content-Type: application/x-www-form-urlencoded; charset=UTF-8");

	con->addHTTP_Header("Accept: application/json");

	if (post_json) {
		con->enqueueHTTP_Send(picojson::value(*post_json).serialize());
	}
	con->connect();

	// Connection: Receive
	std::unique_ptr<picojson::value> json(new picojson::value());
	std::unique_ptr<std::string> contents(con->popAll());
	if (!contents) {
		WARN("Nothing received for " << url);
		return nullptr;
	}

	std::string err = picojson::parse(*json, *contents);
	if (!err.empty())
		WARN("PicoJSON error: " << err);

	if (json->get("ok").evaluate_as_boolean()) {
		*json = json->get("result");
	} else {
		WARN(json->serialize(true));
		*json = picojson::value();
	}

	if (json->is<picojson::null>())
		return nullptr;

	return json.release();
}

Channel *ClientTelegram::joinChannelIfNeeded(cstr_t &channel_id)
{
	Channel *c = m_network->getChannel(channel_id);
	if (c)
		return c;

	picojson::object inp;
	inp["chat_id"] = picojson::value(channel_id);
	c = m_network->addChannel(channel_id);

	REQUEST_WRAP(out_admins, "POST", "getChatAdministrators", &inp);
	if (out_admins) {
		//LOG(out_admins->serialize(true));
		for (auto &user : out_admins->get<picojson::array>()) {
			int64_t user_id = user.get("user").get("id").get<int64_t>();
			c->addUser(UserIdTelegram(user_id));
		}
	}

	m_module_mgr->onChannelJoin(c);
	return c;
}

const ClientTelegramActionEntry ClientTelegram::s_actions[] = {
	{ "message", &ClientTelegram::handleMessage },
	{ nullptr, nullptr }
};

void ClientTelegram::handleUnknown(picojson::value &v)
{
	LOG("Unhandled: " << v.serialize(true));
}

void ClientTelegram::handleMessage(cstr_t &type, picojson::value &v)
{
	if (!v.get("text").is<std::string>())
		return; // Join/leave information
	if (v.get("date").get<int64_t>() < m_start_time) {
		WARN("Outdated last ID! Ignoring.");
		return;
	}
	picojson::value &v_from = v.get("from");
	if (!v_from.is<picojson::object>())
		return; // Global infomation/notices

	std::string channel(std::to_string(v.get("chat").get("id").get<int64_t>()));
	Channel *c = joinChannelIfNeeded(channel);

	int64_t user_id = v_from.get("id").get<int64_t>();
	if (user_id == m_my_user_id)
		return;

	UserInstance *ui = c->getUser(UserIdTelegram(user_id));
	if (!ui) {
		ui = c->addUser(UserIdTelegram(user_id));
		m_module_mgr->onUserJoin(c, ui);
	}
	{
		// Update information
		ui->nickname = v_from.get("first_name").get<std::string>();
		ui->is_bot = v_from.get("is_bot").evaluate_as_boolean();
	}

	std::string msg(v.get("text").get<std::string>());

	{
		// Log this line
		auto &os = g_logger->getStdout(LL_NORMAL);
		write_timestamp(&os);
		os << " ";
		os << ui->nickname << " \t";
		os << msg << std::endl;
	}

	m_module_mgr->onUserSay(c, ui, msg);
	if (msg == "PING")
		actionSay(c, "PONG");
}
