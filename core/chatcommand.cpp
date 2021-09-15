#include "chatcommand.h"
#include "logger.h"
#include "utils.h"
#include <sstream>

ChatCommand::ChatCommand(IModule *module)
{
	m_module = module;
}

void ChatCommand::add(cstr_t &subcmd, ChatCommandAction action, IModule *module)
{
	ChatCommand &cmd = add(subcmd, module);
	cmd.setMain(action);
}

ChatCommand &ChatCommand::add(cstr_t &subcmd, IModule *module)
{
	ASSERT(module || m_module, "ChatCommand instance must be owned by a module");

	auto [it, is_new] = m_subs.insert({subcmd, ChatCommand(module ? module : m_module)});

	if (!is_new) {
		WARN("Overriding command " << subcmd << "!");
		m_subs.clear();
	}

	return it->second;
}

const ChatCommand *ChatCommand::getParent(ChatCommandAction action) const
{
	for (const auto &it : m_subs) {
		if (it.second.m_action == action)
			return this;

		const ChatCommand *cmd = it.second.getParent(action);
		if (cmd)
			return cmd;
	}

	return nullptr;
}

void ChatCommand::remove(IModule *module)
{
	// Remove all affected subcommands
	for (auto it = m_subs.begin(); it != m_subs.end(); ) {
		if (it->second.m_module == module)
			m_subs.erase(it++);
		else
			++it;
	}

	if (m_module == module) {
		m_module = nullptr;
		m_action = nullptr;
	}
}

bool ChatCommand::run(Channel *c, UserInstance *ui, std::string &msg) const
{
	// Main command
	if (m_module && (msg.empty() || m_subs.empty())) {
		(m_module->*m_action)(c, ui, msg);
		return true;
	}

	std::string cmd(get_next_part(msg));

	auto it = m_subs.find(cmd);
	if (it != m_subs.end()) {
		it->second.run(c, ui, msg);
		return true;
	}

	// Show help function if available
	if (m_module) {
		(m_module->*m_action)(c, ui, msg);
		return true;
	}
	return false;
}

std::string ChatCommand::getList() const
{
	bool first = true;
	std::ostringstream oss;
	for (const auto &it : m_subs) {
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
