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

	bool onUserSay(Channel *c, UserInstance *ui, std::string &msg)
	{
		std::string cmd(get_next_part(msg));
		if (cmd.size() < 2 || cmd[0] != '$')
			return false;

		if (cmd == "$reload") {
			if (!isBotAdmin(c, ui))
				return true;

			std::string module(get_next_part(msg));
			std::string keep(get_next_part(msg));

			getModuleMgr()->reloadModule(module, is_yes(keep));
			c->notice(ui, "Enqueued!");
			return true;
		}
		if (cmd == "$quit") {
			if (!isBotAdmin(c, ui))
				return true;

			sendRaw("QUIT :Goodbye!");
			return true;
		}
		if (cmd == "$list") {
			std::string list("List: ");
			for (UserInstance *ui : c->getAllUsers()) {
				list.append(ui->nickname);
				list.append("_");
			}
			c->say(list);
			return true;
		}
		if (cmd == "$updateauth") {
			addClientRequest(ClientRequest {
				.type = ClientRequest::RT_STATUS_UPDATE,
				.status_update_nick = new std::string(ui->nickname)
			});
			return true;
		}

		return false;
	}

	bool isBotAdmin(Channel *c, UserInstance *ui)
	{
		cstr_t &admin = getModuleMgr()->getGlobalSettings()->get("client.admin");
		if (ui->nickname == admin && ui->account == UserInstance::UAS_LOGGED_IN)
			return true;

		c->reply(ui, "Insufficient privileges.");
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
	Settings *m_settings = nullptr;
	ChatCommand *m_commands = nullptr;
};

extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_builtin();
	}
}
