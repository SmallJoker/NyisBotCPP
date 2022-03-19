#include "../core/channel.h"
#include "../core/chatcommand.h"
#include "../core/logger.h"
#include "../core/module.h"
#include "../core/settings.h"
#include <sstream>

struct TellRecord : public SettingType {
	bool deSerialize(cstr_t &str)
	{
		if (str.size() < 5)
			return false;

		const char *pos = str.c_str();

		return parseS64(&pos, &timestamp)
			&& parseString(&pos, &nick_src)
			&& parseString(&pos, &nick_dst)
			&& parseString(&pos, &text, 0);
	}

	std::string serialize() const
	{
		std::ostringstream os;
		os << timestamp << " " << nick_src << " " << nick_dst << " " << text;
		return os.str();
	}

	int64_t timestamp;
	std::string nick_src;
	std::string nick_dst;
	std::string text;
};

class nbm_tell : public IModule {
public:
	~nbm_tell()
	{
		if (!m_settings)
			return;

		m_settings->syncFileContents(SR_WRITE);
	}

	void onClientReady()
	{
		m_settings = getModuleMgr()->getSettings(this);

		getModuleMgr()->getChatCommand()->add("$tell", (ChatCommandAction)&nbm_tell::cmd_tell, this);
	}

	void onStep(float time)
	{
		thread_local static float cleanup_tick = 9999;
		for (auto it = m_cooldown.begin(); it != m_cooldown.end();){
			it->second -= time;
			if (it->second <= 0.0f)
				m_cooldown.erase(it++);
			else
				++it;
		}

		// Remove old messages
		cleanup_tick += time;
		if (cleanup_tick > 3600.0f) {
			cleanup_tick = 0;

			int64_t now = std::time(nullptr);
			auto keys = m_settings->getKeys();
			for (cstr_t &key : keys) {
				TellRecord tr;
				if (m_settings->get(key, &tr)) {
					if (now - tr.timestamp < EXPIRY_TIME)
						continue; // Keep
				}
				// Invalid key
				m_settings->remove(key);
			}
			m_settings->syncFileContents(SR_WRITE);
		}
	}

	void tellTell(Channel *c, UserInstance *ui)
	{
		auto keys = m_settings->getKeys();
		for (cstr_t &key : keys) {
			TellRecord tr;
			if (m_settings->get(key, &tr)) {
				if (!strequalsi(tr.nick_dst, ui->nickname))
					continue;

				std::ostringstream os;
				os << "From " << tr.nick_src << " at [";
				write_datetime(&os);
				os << "]: " << tr.text;
				c->reply(ui, os.str());
			} else {
				WARN("Invalid key: " << key);
			}
			m_settings->remove(key);
		}
	}

	void onUserJoin(Channel *c, UserInstance *ui)
	{
		tellTell(c, ui);
	}
	
	bool onUserSay(Channel *c, UserInstance *ui, std::string &msg)
	{
		tellTell(c, ui);
		return false;
	}

	CHATCMD_FUNC(cmd_tell)
	{
		if (m_cooldown.find(ui) != m_cooldown.end()) {
			c->reply(ui, "Please wait a bit before leaving another message.");
			return;
		}
	
		std::string nick(get_next_part(msg));

		if (msg.size() < 10) {
			c->reply(ui, "Message is too short");
			return;
		}

		char buf[30];
		snprintf(buf, sizeof(buf), "msg_%x", hashELF32(msg.c_str(), msg.size()));
		std::string key(buf);

		{
			TellRecord tr;
			if (m_settings->get(key, &tr)) {
				c->reply(ui, "The same message is already enqueued.");
				return;
			}
		}

		// TODO: This is bullshit. Use sqlite3
		{
			TellRecord tr;
			tr.nick_src = ui->nickname;
			tr.nick_dst = nick;
			tr.text = msg;
			if (m_settings->set(key, &tr))
				c->reply(ui, "OK!");
			else
				c->reply(ui, "Internal error");
		}
		m_cooldown[ui] = COOLDOWN_TIME;
	}

private:
	Settings *m_settings = nullptr;

	static const int64_t EXPIRY_TIME = 3600 * 24 * 20; // 20 days
	static const int COOLDOWN_TIME = 40; // seconds
	std::map<void *, double> m_cooldown;
};

extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_tell();
	}
}
