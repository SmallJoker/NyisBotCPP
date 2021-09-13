#include "../core/channel.h"
#include "../core/module.h"
#include "../core/logger.h"

#include "../core/client.h"

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
	virtual void onUserRename(Channel *c, UserInstance *ui, cstr_t &new_name) {}

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
		if (cmd == "$quit") {
			m_client->send("QUIT :Goodbye!");
			return true;
		}

		return false;
	}
};

extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_builtin();
	}
}
