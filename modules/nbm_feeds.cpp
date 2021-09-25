#include "channel.h"
#include "chatcommand.h"
#include "connection.h"
#include "logger.h"
#include "module.h"
#include "settings.h"
#include "utils.h"
#include <iostream>
#include <memory>
#include <pugixml.hpp>


struct Feeds : public IContainer {
	~Feeds()
	{
		if (settings)
			settings->syncFileContents();
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
			m_settings->syncFileContents();
		delete m_settings;
	}

	void onClientReady()
	{
		m_settings = getModuleMgr()->getSettings(this);
	}

	void initCommands(ChatCommand &cmd)
	{
		ChatCommand &lcmd = cmd.add("$feeds", this);
		lcmd.setMain((ChatCommandAction)&nbm_feeds::cmd_help);
		lcmd.add("list",   (ChatCommandAction)&nbm_feeds::cmd_list, this);
		lcmd.add("add",    (ChatCommandAction)&nbm_feeds::cmd_add, this);
		lcmd.add("remove", (ChatCommandAction)&nbm_feeds::cmd_remove, this);
		m_commands = &lcmd;
	}

	void onStep(float time)
	{
		timer += time;
		if (timer < 15 * 60)
			return;
		timer -= 15 * 60;

		for (Channel *c : getNetwork()->getAllChannels()) {
			Feeds *f = getFeedsOrCreate(c);

			// Iterate through all registered feeds
			auto keys = f->settings->getKeys();
			for (cstr_t &key : keys)
				notifySingle(c, key, f);

			f->settings->syncFileContents();
		}
	}

	void onChannelLeave(Channel *c)
	{
		delete c->getContainers()->get(this);
	}

	inline Feeds *getFeedsOrCreate(Channel *c)
	{
		if (c->isPrivate())
			return nullptr;

		Feeds *f = (Feeds *)c->getContainers()->get(this);
		if (!f) {
			f = new Feeds();
			// Assign required settings prefix -> trim '#' in front
			f->settings = m_settings->fork(c->getName().substr(1));
			c->getContainers()->set(this, f);
		}
		return f;
	}

	void notifySingle(Channel *c, cstr_t &key, Feeds *f)
	{
		cstr_t &url = f->settings->get(key);

		// Download feed, analyze
		std::unique_ptr<Connection> con(Connection::createHTTP_GET(url));
		con->connect();

		std::unique_ptr<std::string> text(con->popAll());
		if (!text) {
			WARN("Download failed");
			return;
		}

		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_string(text->c_str());
		if (!result)
			WARN("XML parser fail: " << result.description());

		auto nodes = doc.select_nodes("/feed/entry");
		for (auto &it : nodes) {
			std::cout << it.node().child("title").text() << std::endl;
		}
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
		// stub
	}

	CHATCMD_FUNC(cmd_add)
	{
		Feeds *f = getFeedsOrCreate(c);
		if (!f)
			return;

		std::string key = "test";
		f->settings->set(key, strtrim(msg));

		notifySingle(c, key, f);
	}

	CHATCMD_FUNC(cmd_remove)
	{
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

