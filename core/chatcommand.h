#pragma once

#include "module.h"
#include "types.h"
#include <map>

class Channel;
class ChatCommand;
class UserInstance;

typedef bool (IModule::*ChatCommandAction)(Channel *c, UserInstance *ui, std::string msg);

class ChatCommand {
public:
	ChatCommand(IModule *module);

	void setMain(ChatCommandAction action)
	{ m_action = action; }

	void add(cstr_t &subcmd);
	void add(cstr_t &subcmd, ChatCommandAction action);

	bool run(Channel *c, UserInstance *ui, std::string &msg);

	std::string getList();

private:
	ChatCommandAction m_action = nullptr;
	IModule *m_module;

	std::map<std::string, ChatCommand> m_subs;
};

