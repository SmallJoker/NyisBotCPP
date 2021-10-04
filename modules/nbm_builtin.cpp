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

		saveSettings();
	}

	void onClientReady()
	{
		m_settings = getModuleMgr()->getSettings(this);

		ChatCommand &cmd = *getModuleMgr()->getChatCommand();
		m_commands = &cmd;
		cmd.add("$help",     (ChatCommandAction)&nbm_builtin::cmd_help,     this);
		cmd.add("$remember", (ChatCommandAction)&nbm_builtin::cmd_remember, this);
		cmd.add("$setting",  (ChatCommandAction)&nbm_builtin::cmd_setting, this);
	}

	void onUserStatusUpdate(UserInstance *ui, bool is_timeout)
	{
		auto it = m_status_updates.find(ui);
		if (it == m_status_updates.end())
			return;

		Channel *c = it->second;
		m_status_updates.erase(it);

		if (!getNetwork()->contains(c))
			return;
		if (!c->contains(ui))
			return;

		if (is_timeout)
			c->notice(ui, "Auth request timed out. Bot error?");
		else
			c->notice(ui, "OK. Updated! Status: " + std::to_string(ui->account));
	}

	bool onUserSay(Channel *c, UserInstance *ui, std::string &msg)
	{
		std::string cmd(get_next_part(msg));
		if (cmd.size() < 2 || cmd[0] != '$')
			return false;

		if (cmd == "$reload") {
			if (!checkBotAdmin(c, ui))
				return true;

			std::string module(get_next_part(msg));
			std::string keep(get_next_part(msg));

			getModuleMgr()->reloadModule(module, is_yes(keep));
			c->notice(ui, "Enqueued!");
			return true;
		}
		if (cmd == "$quit") {
			if (!checkBotAdmin(c, ui))
				return true;

			sendRaw("QUIT :Goodbye!");
			return true;
		}
		if (cmd == "$modules") {
			std::string text("Loaded modules:");
			for (cstr_t &name : getModuleMgr()->getModuleList()) {
				text.append(" " + name);
			}
			c->say(text);
			return true;
		}
		if (cmd == "$updateauth") {
			m_status_updates[ui] = c;
			addClientRequest(ClientRequest {
				.type = ClientRequest::RT_STATUS_UPDATE,
				.status_update = ui
			});
			return true;
		}

		return false;
	}

	void saveSettings()
	{
		if (!m_settings)
			return;

		for (UserInstance *ui : getNetwork()->getAllUsers()) {
			BuiltinContainer *bc = (BuiltinContainer *)ui->get(this);
			if (bc)
				m_settings->set(ui->nickname, bc->remember_text);
		}

		m_settings->syncFileContents(SR_WRITE);
	}

	CHATCMD_FUNC(cmd_help)
	{
		c->say("Available commands: " + m_commands->getList() +
			", $reload <module> [<keep>], $list, $quit, $updateauth");
	}

	CHATCMD_FUNC(cmd_remember)
	{
		std::string what(strtrim(msg));

		BuiltinContainer *bc = (BuiltinContainer *)ui->get(this);
		if (!bc) {
			bc = new BuiltinContainer();
			bc->remember_text = m_settings->get(ui->nickname);
			ui->set(this, bc);
		}

		if (what.empty()) {
			// Retrieve remembered text
			c->say(bc->remember_text);
		} else {
			// Save text
			bc->remember_text = what;
			c->say("Saved!");
		}
	}

	CHATCMD_FUNC(cmd_setting)
	{
		std::string cmd(get_next_part(msg));
		if (cmd == "set") {
			std::string key(get_next_part(msg));
			if (msg.empty())
				m_settings->remove(key);
			else
				m_settings->set(key, strtrim(msg));
			c->notice(ui, "OK!");
			return;
		}
		if (cmd == "get") {
			std::string key(get_next_part(msg));
			c->say(ui->nickname + ": " + m_settings->get(key));
			return;
		}
		if (cmd == "save") {
			saveSettings();
			c->notice(ui, "OK!");
			return;
		}

		c->say("Available commands: set, get, save");
	}

private:
	std::map<UserInstance *, Channel *> m_status_updates;
	Settings *m_settings = nullptr;
	ChatCommand *m_commands = nullptr;
};

extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_builtin();
	}
}
