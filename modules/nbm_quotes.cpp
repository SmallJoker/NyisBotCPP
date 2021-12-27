#include "../core/channel.h"
#include "../core/chatcommand.h"
#include "../core/logger.h"
#include "../core/module.h"
#include "../core/settings.h"
#include <algorithm> // std::find
#include <sstream>

struct Quote : public SettingType {
	bool deSerialize(cstr_t &str)
	{
		if (str.size() < 5)
			return false;
		text = str;
		writer = get_next_part(text);
		return true;
	}

	std::string serialize() const
	{
		return writer + " " + text;
	}

	std::string writer;
	std::string text;
};

class nbm_quotes : public IModule {
public:
	~nbm_quotes()
	{
		if (!m_settings)
			return;

		m_settings->syncFileContents(SR_WRITE);
	}

	void onClientReady()
	{
		m_settings = getModuleMgr()->getSettings(this);
		auto keys = m_settings->getKeys();
		if (std::find(keys.begin(), keys.end(), "id") != keys.end()) {
			long id = 0;
			SettingType::parseLong(m_settings->get("id"), &id);
			if (id >= 1)
				m_last_id = id;
		}

		ChatCommand &cmd = getModuleMgr()->getChatCommand()->add("$quote", this);
		cmd.add("add",    (ChatCommandAction)&nbm_quotes::cmd_add, this);
		cmd.add("get",    (ChatCommandAction)&nbm_quotes::cmd_get, this);
		cmd.add("remove", (ChatCommandAction)&nbm_quotes::cmd_remove, this);
		m_subcommands = &cmd;
	}

	void onStep(float time)
	{
		for (auto it = m_cooldown.begin(); it != m_cooldown.end();){
			it->second -= time;
			if (it->second <= 0.0f)
				m_cooldown.erase(it++);
			else
				++it;
		}
	}

	CHATCMD_FUNC(cmd_help)
	{
		c->say("Available commands: " + m_subcommands->getList());
	}

	CHATCMD_FUNC(cmd_add)
	{
		if (m_cooldown.find(c) != m_cooldown.end()
				|| m_cooldown.find(ui) != m_cooldown.end()) {
			c->reply(ui, "Quotes are rate limited by "
				+ std::to_string(COOLDOWN_TIME) + " seconds per channel and user");
			return;
		}
		if (msg.size() < 10) {
			c->reply(ui, "Quote is too short");
			return;
		}
		auto nick_a = msg.find('<');
		auto nick_b = msg.find('>');
		if (nick_a == std::string::npos || nick_b == std::string::npos || nick_a > nick_b) {
			c->reply(ui, "Quote must have the following format: <nickname> text ...");
			return;
		}

		auto keys = m_settings->getKeys();
		for (cstr_t &key : keys) {
			if (m_settings->get(key).rfind(msg) == std::string::npos)
				continue;

			c->reply(ui, "Found similar quote: #" + key);
			return;
		}

		std::string id(std::to_string(m_last_id));
		std::string nickname = ui->nickname;
		Settings::sanitizeKey(nickname);

		Quote q;
		q.writer = nickname;
		q.text = msg;
		m_settings->set(id, &q);
		m_settings->set("id", id);
		c->reply(ui, "Added as #" + id);
		m_last_id++;

		m_cooldown[c] = COOLDOWN_TIME;
		m_cooldown[ui] = COOLDOWN_TIME;
	}

	CHATCMD_FUNC(cmd_get)
	{
		auto keys = m_settings->getKeys();
		std::remove_if(keys.begin(), keys.end(), [] (auto &str) -> bool {
			return str == "id";
		});
		std::vector<std::string> matches;

		if (msg.empty())
			matches.push_back(keys[get_random() % keys.size()]);

		if (matches.empty()) {
			// Search by ID
			long id = -1;
			if (SettingType::parseLong(msg, &id))
				matches.push_back(std::to_string(id));
		}

		if (matches.empty()) {
			// Search by text
			for (cstr_t &key : keys) {
				if (strfindi(m_settings->get(key), msg) != std::string::npos)
					matches.push_back(key);
			}
		}

		if (matches.empty()) {
			c->reply(ui, "No matches. Either specify an ID or text to search");
			return;
		}
		if (matches.size() == 1) {
			Quote q;
			if (m_settings->get(matches[0], &q))
				c->say(colorize_string("[Quote #" + matches[0] + "] ", IC_ORANGE) + q.text);
			else
				c->reply(ui, "Unknown or invalid quote #" + matches[0]);
			return;
		}

		std::sort(matches.begin(), matches.end());
		std::ostringstream ss;
		ss << "Found following matches: ";
		for (size_t i = 0; i < matches.size(); ++i) {
			ss << "#" << matches[i];
			if (i + 1 < matches.size())
				ss << ", ";
		}
		c->reply(ui, ss.str());
	}

	CHATCMD_FUNC(cmd_remove)
	{
		c->reply(ui, "stub");
	}

private:
	size_t m_last_id = 1;
	Settings *m_settings = nullptr;
	ChatCommand *m_subcommands = nullptr;

	static const int COOLDOWN_TIME = 40; // seconds
	std::map<void *, double> m_cooldown;
};

extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_quotes();
	}
}

