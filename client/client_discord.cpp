#include "client_discord.h"
#include "connection.h"
#include "logger.h"
#include "settings.h"
#include <memory>
#include <picojson.h>
#include <sstream>

static const std::string BASE_URL("https://discord.com/api/v9");

static void snowflake_analysis(const uint64_t num, time_t *timestamp,
	uint8_t *worker_id, uint8_t *process_id, uint16_t *increment)
{
	// https://discord.com/developers/docs/reference
	*timestamp = (num >> 22) + 1420070400000ULL;
	*worker_id = (num & 0x3E0000) >> 17;
	*process_id = (num & 0x1F000) >> 12;
	*increment = num & 0xFFF;
}

ClientDiscord::ClientDiscord(Settings *settings) :
	IClient(settings)
{
	
}

ClientDiscord::~ClientDiscord()
{
}

void ClientDiscord::initialize()
{
	// Authenticate

	m_status = DS_CONNECT;
}

#define REQUEST_WRAP(var, arg1, arg2, arg3) \
	std::unique_ptr<picojson::value> var((picojson::value *)requestREST(arg1, arg2, arg3))

bool ClientDiscord::run()
{
	const DiscordStatus status = m_status;
	m_status = DS_IDLE;

	switch (status) {
		case DS_IDLE: break;
		case DS_CONNECT: {
			picojson::object in;

			// https://discord.com/developers/docs/topics/gateway
			// The server returns status code 403. Why?
			std::unique_ptr<Connection> con(Connection::createWebsocket(
					"https://gateway.discord.gg/?v=9&encoding=json"));
			con->connect();

			picojson::value out;
			while (true) {
				sleep_ms(200);

				std::unique_ptr<std::string> contents(con->popRecv());
				if (!contents)
					continue;

				LOG(*contents);
				std::string err = picojson::parse(out, *contents);
				if (!err.empty()) {
					WARN("PicoJSON error: " << err);
					continue;
				}
			}

		} break;
		case DS_OBTAIN_TOKEN: {
			// 1. Add bot to channel with oauth2 URL
			// 2. Obtain token

			// https://discord.com/developers/docs/topics/oauth2
			picojson::object in;
			in["client_id"] = picojson::value(m_settings->get("discord.client_id"));
			in["client_secret"] = picojson::value(m_settings->get("discord.client_secret"));
			in["grant_type"] = picojson::value("authorization_code");
			in["code"] = picojson::value("????");
			in["redirect_uri"] = picojson::value("????");
			LOG(picojson::value(in).serialize(true));

			REQUEST_WRAP(out, "POST", BASE_URL + "/oauth2/token", &in);
			LOG(out->serialize(true));
		} break;
		case DS_STATUSINFO: {
			REQUEST_WRAP(out, "GET", BASE_URL + "/oauth2/applications/@me", nullptr);
			LOG(out->serialize(true));
		} break;
	}
	return true;
}

void *ClientDiscord::requestREST(cstr_t &method, cstr_t &url, void *post_json)
{
	// Testing: https://jsonplaceholder.typicode.com/guide/

	std::unique_ptr<Connection> con(Connection::createHTTP(method, url));
	// Connection: Send
	if (post_json)
		con->addHTTP_Header("Content-Type: application/json; charset=UTF-8");

	con->addHTTP_Header("Accept: application/json");

	if (!m_token.empty())
		con->addHTTP_Header("Authorization: Bot " + m_token);

	if (post_json) {
		picojson::object *json = (picojson::object *)post_json;
		con->enqueueHTTP_Send(picojson::value(*json).serialize());
	}
	con->connect();

	// Connection: Receive
	auto json = new picojson::value();
	std::unique_ptr<std::string> contents(con->popAll());
	if (!contents) {
		WARN("Nothing received for " << url);
		return json;
	}

	std::string err = picojson::parse(*json, *contents);
	if (!err.empty())
		WARN("PicoJSON error: " << err);

	return json;
}
