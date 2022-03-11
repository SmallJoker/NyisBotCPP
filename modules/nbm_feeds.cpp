#include "channel.h"
#include "chatcommand.h"
#include "connection.h"
#include "logger.h"
#include "module.h"
#include "settings.h"
#include "utils.h"
//#include <fstream>
#include <iostream>
#include <memory>
#include <pugixml.hpp>

struct Feeds : public IContainer {
	std::string dump() const { return "Feeds"; }

	~Feeds()
	{
		if (settings)
			settings->syncFileContents(SR_WRITE);
		delete settings;
	}

	Settings *settings = nullptr;
	std::map<std::string, time_t> last_update;
};

class nbm_feeds : public IModule {
public:
	~nbm_feeds()
	{
		if (m_settings)
			m_settings->syncFileContents(SR_WRITE);
	}

	void onClientReady()
	{
		m_settings = getModuleMgr()->getSettings(this);

		ChatCommand &lcmd = getModuleMgr()->getChatCommand()->add("$feeds", this);
		lcmd.setMain((ChatCommandAction)&nbm_feeds::cmd_help);
		lcmd.add("list",   (ChatCommandAction)&nbm_feeds::cmd_list, this);
		lcmd.add("add",    (ChatCommandAction)&nbm_feeds::cmd_add, this);
		lcmd.add("remove", (ChatCommandAction)&nbm_feeds::cmd_remove, this);
		m_commands = &lcmd;
	}

	void onStep(float time)
	{
		timer += time;
		if (timer < 30 * 60)
			return;
		timer -= 30 * 60;

		size_t count = 0;
		for (Channel *c : getNetwork()->getAllChannels()) {
			Feeds *f = getFeedsOrCreate(c);

			// Iterate through all registered feeds
			auto keys = f->settings->getKeys();
			for (cstr_t &key : keys) {
				const char *err = notifySingle(c, key, f);
				if (err)
					WARN(err);
				count++;
			}
		}
		LOG("Feed check completed (" << count << " feeds)");
	}

	void onChannelLeave(Channel *c)
	{
		c->getContainers()->remove(this);
	}

	inline Feeds *getFeedsOrCreate(Channel *c)
	{
		if (c->isPrivate())
			return nullptr;

		Feeds *f = (Feeds *)c->getContainers()->get(this);
		if (!f) {
			f = new Feeds();
			// Assign required settings prefix
			f->settings = m_settings->fork(c->getSettingsName());
			c->getContainers()->set(this, f);
		}
		return f;
	}

	const char *notifySingle(Channel *c, cstr_t &key, Feeds *f)
	{
#if 1
		cstr_t &url = f->settings->get(key);
		if (url.empty())
			return "Invalid request. Empty URL.";

		// Download feed, analyze
		std::unique_ptr<Connection> con(Connection::createHTTP("GET", url));
		con->connect();

		std::unique_ptr<std::string> text(con->popAll());
		if (!text) {
			f->last_update.erase(key);
			return "File download failed";
		}

		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_buffer_inplace(&(*text)[0], text->size());
#else
		pugi::xml_document doc;
		std::ifstream file("example_feed.txt");
		pugi::xml_parse_result result = doc.load(file);
#endif

		if (!result) {
			WARN("XML parser failed: " << result.description());
			return "XML parser failed";
		}

		time_t last_check = 0;
		{
			auto it = f->last_update.find(key);
			if (it != f->last_update.end())
				last_check = it->second;
		}
		time_t time_newest = last_check;

		auto nodes = doc.select_nodes("/feed/entry");
		if (nodes.empty())
			return "Cannot find any feed entries";

		int msg_limit = 3;
		for (auto &it : nodes) {
			std::string title(strtrim(it.node().child_value("title")));
			auto link = it.node().child("link").attribute("href").as_string();
			auto author = it.node().child("author").child_value("name");
			auto date = it.node().child_value("updated");

			tm timeinfo;
			if (!strptime(date, "%FT%TZ", &timeinfo)) {
				VERBOSE("Invalid date: " << title << " date=" << date);
				continue; // Invalid date
			}

			time_t timestamp = mktime(&timeinfo);
			if (timestamp <= last_check)
				continue; // Skip old information

			if (timestamp > time_newest)
				time_newest = timestamp;

			if (last_check == 0) {
				// Init cycle. Update to newest timestamp
				continue;
			}

			if (msg_limit-- == 0) {
				LOG("Limit reached!");
				break; // Rate limit
			}

			auto parts = strsplit(link, '/');
			// AUTHOR @PROJECTNAME: COMMIT MESSAGE
			auto *fmt = c->createFormatter();

			fmt->begin(IC_GREEN); *fmt << author; fmt->end(FT_COLOR);
			*fmt << " @";
			fmt->begin(IC_MAROON); *fmt << parts[3]; fmt->end(FT_COLOR);
			*fmt << ": ";
			fmt->begin(IC_LIGHT_GRAY); *fmt << title; fmt->end(FT_COLOR);
	
			c->say(fmt->str());
			delete fmt;
		}
		f->last_update[key] = time_newest;

		return nullptr;
	}

	CHATCMD_FUNC(cmd_help)
	{
		c->say("Available subcommands: " + m_commands->getList());
	}

	CHATCMD_FUNC(cmd_list)
	{
		Feeds *f = getFeedsOrCreate(c);
		if (!f)
			return;

		std::string out("List of feeds: ");
		auto keys = f->settings->getKeys();
		for (cstr_t &key : keys)
			out.append(key + " ");

		c->say(out);
	}

	CHATCMD_FUNC(cmd_add)
	{
		if (!checkBotAdmin(c, ui))
			return;
		Feeds *f = getFeedsOrCreate(c);
		if (!f)
			return;

		std::string key(get_next_part(msg));
		if (!Settings::isKeyValid(key)) {
			c->reply(ui, "Key contains invalid characters. Allowed are: [A-z0-9_.]");
			return;
		}
		if (msg.empty()) {
			c->reply(ui, "URL is required. $feeds add <name> <URL>");
			return;
		}

		f->settings->set(key, strtrim(msg));

		// Test it.
		const char *err = notifySingle(c, key, f);
		if (err) {
			c->say("Feed test failed: " + std::string(err));
			f->settings->remove(key);
		} else {
			c->reply(ui, "Added!");
		}
		f->settings->syncFileContents(SR_WRITE);
	}

	CHATCMD_FUNC(cmd_remove)
	{
		if (!checkBotAdmin(c, ui))
			return;
		Feeds *f = getFeedsOrCreate(c);
		if (!f)
			return;

		if (f->settings->remove(strtrim(msg))) {
			c->say("Success!");
		} else {
			c->say("Key not found");
		}
		f->settings->syncFileContents(SR_WRITE);
	}

private:
	ChatCommand *m_commands = nullptr;
	Settings *m_settings = nullptr;
	float timer = 0;
};


extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_feeds();
	}
}

