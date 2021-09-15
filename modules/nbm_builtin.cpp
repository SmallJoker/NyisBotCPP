#include "../core/channel.h"
#include "../core/chatcommand.h"
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

	void initCommands(ChatCommand &cmd)
	{
		m_chatcommands = &cmd;
		cmd.add("$help",     (ChatCommandAction)&nbm_builtin::cmd_help,     this);
		cmd.add("$remember", (ChatCommandAction)&nbm_builtin::cmd_remember, this);
	}

	void onClientReady()
	{
		if (m_settings)
			return;

		m_settings = getModuleMgr()->getSettings(this);
		m_settings->syncFileContents();
	}

	void onModuleUnload()
	{
		if (!m_settings)
			return;

		for (UserInstance *ui : getNetwork()->getAllUsers()) {
			BuiltinContainer *bc = (BuiltinContainer *)ui->data->get(this);
			if (bc)
				m_settings->set(ui->nickname, bc->remember_text);
		}
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

	bool onUserSay(Channel *c, UserInstance *ui, std::string &msg)
	{
		std::string cmd(get_next_part(msg));
		if (cmd.size() < 2 || cmd[0] != '$')
			return false;

		if (cmd == "$reload") {
			std::string module(get_next_part(msg));
			std::string keep(get_next_part(msg));

			getModuleMgr()->reloadModule(module, is_yes(keep));
			c->say(ui->nickname + ": Enqueued!");
			return true;
		}
		if (cmd == "$set") {
			std::string key(get_next_part(msg));
			m_settings->set(key, strtrim(msg));
			return true;
		}
		if (cmd == "$get") {
			std::string key(get_next_part(msg));
			c->say(ui->nickname + ": " + m_settings->get(key));
			return true;
		}
		if (cmd == "$save") {
			onModuleUnload();
			return true;
		}
		if (cmd == "$quit") {
			sendRaw("QUIT :Goodbye!");
			return true;
		}

		return false;
	}

	CHATCMD_FUNC(cmd_help)
	{
		c->say("Available commands: " + m_chatcommands->getList());
		return true;
	}

	CHATCMD_FUNC(cmd_remember)
	{
		std::string what(strtrim(msg));

		BuiltinContainer *bc = (BuiltinContainer *)ui->data->get(this);
		if (!bc) {
			bc = new BuiltinContainer();
			bc->remember_text = m_settings->get(ui->nickname);
			ui->data->add(this, bc);
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

private:
	Settings *m_settings = nullptr;
	ChatCommand *m_chatcommands = nullptr;
};

extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_builtin();
	}
}
