#pragma once

#include "module.h"
#include "types.h"
#include <map>

class Channel;
class ChatCommand;
class UserInstance;

#define CHATCMD_FUNC(name) \
	void (name)(Channel *c, UserInstance *ui, std::string msg)

typedef CHATCMD_FUNC(IModule::*ChatCommandAction);

class ChatCommand {
public:
	ChatCommand(IModule *module);

	void setMain(ChatCommandAction action)
	{ m_action = action; }

	void         add(cstr_t &subcmd, ChatCommandAction action, IModule *module = nullptr);
	ChatCommand &add(cstr_t &subcmd, IModule *module = nullptr);
	void remove(IModule *module);

	void setScope(Channel *c, const UserInstance *ui);
	void resetScope(Channel *c, const UserInstance *ui);
	bool run(Channel *c, UserInstance *ui, std::string msg, bool is_scope = false) const;

	std::string getList() const;

private:
	ChatCommandAction m_action = nullptr;
	IModule *m_module;
	ChatCommand *m_root = nullptr;

	std::map<std::string, ChatCommand> m_subs;
};

