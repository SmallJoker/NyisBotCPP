#include "client_discord.h"
#include "connection.h"
#include "logger.h"
#include "settings.h"
#include <memory>
#include <picojson.h>
#include <sstream>

static const char *BASE_URL = "https://discord.com/api/v8";

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
	// https://discord.com/developers/docs/topics/oauth2
	cstr_t &url = m_settings->get("discord.oauth2_url");
}

ClientDiscord::~ClientDiscord()
{
}

void ClientDiscord::initialize()
{
	picojson::object v;
	v["id"]     = picojson::value(2.0);
	v["title"]  = picojson::value("foo");
	v["body"]   = picojson::value("bar");
	v["userId"] = picojson::value(100.0);

	auto json = (picojson::value *)requestREST("DELETE", "https://jsonplaceholder.typicode.com/posts/1", &v);
	std::cout << json->serialize(true) << std::endl;
}

bool ClientDiscord::run()
{
    // https://os.mbed.com/users/mimil/code/PicoJSONSample//file/81c978de0e2b/main.cpp/
	picojson::object v;
	//v["test"] = picojson::value(1);
	return false;
}

void *ClientDiscord::requestREST(cstr_t &method, cstr_t &url, void *post_json)
{
	// Testing: https://jsonplaceholder.typicode.com/guide/

	std::unique_ptr<Connection> con(Connection::createHTTP(method, url));
	// Connection: Send
	con->addHTTP_Header("Content-Type: application/json; charset=UTF-8");
	con->addHTTP_Header("Accept: application/json");

	if (!m_token.empty())
		con->addHTTP_Header("Authorization: " + m_token);

	if (post_json) {
		picojson::object *json = (picojson::object *)post_json;
		con->setHTTP_Data(picojson::value(*json).serialize());
	}
	con->connect();

	// Connection: Receive
	std::unique_ptr<std::string> contents(con->popAll());
	if (!contents) {
		WARN("Nothing received for " << url);
		return nullptr;
	}

	auto json = new picojson::value();
	std::string err = picojson::parse(*json, *contents);
	if (!err.empty())
		WARN("PicoJSON error: " << err);

	return json;
}
