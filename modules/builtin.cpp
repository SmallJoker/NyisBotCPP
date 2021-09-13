#include "../core/channel.h"
#include "../core/logger.h"
#include "../core/module.h"
#include "../core/settings.h"

struct BuiltinContainer : public IContainer {
	std::string remember_text;
};

class nbm_builtin : public IModule {
public:
	nbm_builtin()
	{
		LOG("Load");
	}
	~nbm_builtin()
	{
		LOG("Unload");
		if (m_settings)
			m_settings->syncFileContents();
		delete m_settings;
	}

	void onClientReady()
	{
		m_settings = getModuleMgr()->getSettings(this);
		m_settings->syncFileContents();
	}

	void onChannelJoin(Channel *c)
	{
		c->say("Hello world!");
	}
	void onUserJoin(Channel *c, UserInstance *ui)
	{
		c->say("Hello " + ui->nickname  + "!");
	}

	virtual void onUserLeave(Channel *c, UserInstance *ui) {}

	virtual void onUserRename(UserInstance *ui, cstr_t &old_name) {}

	bool onUserSay(Channel *c, ChatInfo info)
	{
		std::string cmd(get_next_part(info.text));
		if (cmd.size() < 2 || cmd[0] != '$')
			return false;

		if (cmd == "$help") {
			c->say("Available commands: $reload <module> <keep>, $remember [<text ...>]");
			return true;
		}
		if (cmd == "$reload") {
			std::string module(get_next_part(info.text));
			std::string keep(get_next_part(info.text));

			getModuleMgr()->reloadModule(module, is_yes(keep));
			c->say(info.ui->nickname + ": Enqueued!");
			return true;
		}
		if (cmd == "$remember") {
			std::string what(strtrim(info.text));

			BuiltinContainer *bc = (BuiltinContainer *)info.ui->data->get(this);
			if (!bc) {
				bc = new BuiltinContainer();
				info.ui->data->add(this, bc);
			}

			if (what.empty()) {
				// Retrieve remembered text
				c->say(bc->remember_text);
			} else {
				// Save text
				bc->remember_text = what;
				c->say("Saved!");
			}

			return true;
		}
		if (cmd == "$set") {
			std::string key(get_next_part(info.text));
			m_settings->set(key, strtrim(info.text));
			return true;
		}
		if (cmd == "$get") {
			std::string key(get_next_part(info.text));
			c->say(info.ui->nickname + ": " + m_settings->get(key));
			return true;
		}
		if (cmd == "$save") {
			m_settings->syncFileContents();
			return true;
		}
		if (cmd == "$quit") {
			sendRaw("QUIT :Goodbye!");
			return true;
		}

		return false;
	}

private:
	Settings *m_settings = nullptr;
};

extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_builtin();
	}
}
