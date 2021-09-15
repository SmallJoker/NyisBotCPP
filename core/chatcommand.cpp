#include "chatcommand.h"
#include "logger.h"
#include "utils.h"
#include <sstream>

ChatCommand::ChatCommand(IModule *module)
{
	m_module = module;
}

void ChatCommand::add(cstr_t &subcmd)
{
	auto [it, is_new] = m_subs.insert({subcmd, ChatCommand(m_module)});

	if (!is_new)
		WARN("Overriding command " << subcmd << "!");
}

void ChatCommand::add(cstr_t &subcmd, ChatCommandAction action)
{
	auto [it, is_new] = m_subs.insert({subcmd, ChatCommand(m_module)});
	it->second.setMain(action);

	if (!is_new)
		WARN("Overriding command " << subcmd << "!");
}

bool ChatCommand::run(Channel *c, UserInstance *ui, std::string &msg)
{
	if (msg.empty() || m_subs.empty())
		return m_action && (m_module->*m_action)(c, ui, msg);

	std::string cmd(get_next_part(msg));
	auto it = m_subs.find(cmd);
	return (it != m_subs.end()) && it->second.run(c, ui, msg);
}

std::string ChatCommand::getList()
{
	bool first = true;
	std::ostringstream oss;
	for (auto it : m_subs) {
		if (!first)
			oss << ", ";

		oss << it.first;
		if (!it.second.m_subs.empty()) {
			// Contains subcommands
			oss << " [+...]";
		}
		first = false;
	}
	return oss.str();
}
