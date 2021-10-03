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

		for (Channel *c : getNetwork()->getAllChannels()) {
			Feeds *f = getFeedsOrCreate(c);

			// Iterate through all registered feeds
			auto keys = f->settings->getKeys();
			for (cstr_t &key : keys) {
				const char *err = notifySingle(c, key, f);
				if (err)
					WARN(err);
			}

			f->settings->syncFileContents();
		}
		LOG("Feed check completed");
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
			f->settings->syncFileContents(); // not necessary, but do anyway
			c->getContainers()->set(this, f);
		}
		return f;
	}

	bool isBotAdmin(Channel *c, UserInstance *ui)
	{
		cstr_t &admin = getModuleMgr()->getGlobalSettings()->get("client.admin");
		if (ui->nickname == admin && ui->account == UserInstance::UAS_LOGGED_IN)
			return true;

		c->reply(ui, "Insufficient privileges.");
		return false;
	}

	const char *notifySingle(Channel *c, cstr_t &key, Feeds *f)
	{
#if 1
		cstr_t &url = f->settings->get(key);

		// Download feed, analyze
		std::unique_ptr<Connection> con(Connection::createHTTP("GET", url));
		con->connect();

		std::unique_ptr<std::string> text(con->popAll());
		if (!text) {
			f->settings->remove(key);
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

		time_t last_update = time(nullptr);
		time_t time_newest = 0;
		{
			auto it = f->last_update.find(key);
			if (it != f->last_update.end())
				last_update = it->second;
		}

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
				LOG("Invalid date: " << title << " date=" << date);
				continue; // Invalid date
			}

			time_t timestamp = mktime(&timeinfo);
			if (timestamp <= last_update) {
				LOG("Skip: " << title << " dtime=" << (timestamp - last_update));
				continue; // Skip old information
			}

			if (msg_limit-- == 0) {
				LOG("Limit reached!");
				break; // Rate limit
			}

			auto parts = strsplit(link, '/');
			std::string out;
			out.append(colorize_string(author, IC_GREEN));
			out.append(" @").append(colorize_string(parts[3], IC_MAROON)); // @projectname
			out.append(": ").append(colorize_string(title, IC_LIGHT_GRAY)); // : title
			c->say(out);

			if (timestamp > time_newest)
				time_newest = timestamp;
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
		if (!isBotAdmin(c, ui))
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
			return;
		}
		c->say(ui->nickname + ": Added!");
	}

	CHATCMD_FUNC(cmd_remove)
	{
		if (!isBotAdmin(c, ui))
			return;
		Feeds *f = getFeedsOrCreate(c);
		if (!f)
			return;

		if (f->settings->remove(strtrim(msg))) {
			c->say("Success!");
		} else {
			c->say("Key not found");
		}
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

