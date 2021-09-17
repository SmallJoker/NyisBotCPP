#include "chatcommand.h"
#include "channel.h" // UserInstance
#include "logger.h"
#include "utils.h"
#include <sstream>

// Per-channel data
struct UserCmdScopes : public IContainer {
	std::string dump() const { return "UserCmdScopes"; }

	std::map<const UserInstance *, const ChatCommand *> cmds;
};

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
	// IMPORTANT: Also clean up setScope() !
}

void ChatCommand::setScope(Channel *c, const UserInstance *ui, const ChatCommand *cmd)
{
	if (m_module) {
		ERROR("setScope may only be called on the root instance.");
		return;
	}

	UserCmdScopes *scope = (UserCmdScopes *)c->getContainers()->get(this);
	if (!cmd) {
		VERBOSE("remove " << (ui ? "user-specific" : "all"));

		// Remove all data or single
		if (ui == nullptr)
			c->getContainers()->remove(this);
		else if (scope)
			scope->cmds.erase(ui);
		return;
	}

	if (!scope) {
		scope = new UserCmdScopes();
		c->getContainers()->set(this, scope);
	}

	VERBOSE("set for " << ui->nickname);
	scope->cmds.insert({ ui, cmd });
}

bool ChatCommand::run(Channel *c, UserInstance *ui, std::string &msg) const
{
	do {
		// Main instance only
		if (m_module)
			break;

		UserCmdScopes *scope = (UserCmdScopes *)c->getContainers()->get(this);
		if (!scope || msg[0] != '$')
			break;

		auto it = scope->cmds.find(ui);
		if (it == scope->cmds.end())
			break;

		// Found a shortcut!
		std::string subbed(msg.substr(1));
		if (it->second->run(c, ui, subbed))
			return true;
	} while (0);

	// Main command
	if (m_module && m_action && (msg.empty() || m_subs.empty())) {
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
	if (m_module && m_action) {
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
