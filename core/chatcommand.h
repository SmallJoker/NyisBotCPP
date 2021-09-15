#pragma once

#include "module.h"
#include "types.h"
#include <map>

class Channel;
class ChatCommand;
class UserInstance;

#define CHATCMD_FUNC(name) \
	bool (name)(Channel *c, UserInstance *ui, std::string msg)

typedef CHATCMD_FUNC(IModule::*ChatCommandAction);

class ChatCommand {
public:
	ChatCommand(IModule *module);

	void setMain(ChatCommandAction action)
	{ m_action = action; }

	void         add(cstr_t &subcmd, ChatCommandAction action, IModule *module = nullptr);
	ChatCommand &add(cstr_t &subcmd, IModule *module = nullptr);
	const ChatCommand *getParent(ChatCommandAction action) const;
	void remove(IModule *module);

	bool run(Channel *c, UserInstance *ui, std::string &msg) const;

	std::string getList() const;

private:
	ChatCommandAction m_action = nullptr;
	IModule *m_module;

	std::map<std::string, ChatCommand> m_subs;
};

