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
	if (!module)
		m_root = this;
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
	it->second.m_root = m_root;

	if (!is_new) {
		WARN("Overriding command " << subcmd << "!");
		m_subs.clear();
	}

	return it->second;
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

void ChatCommand::setScope(Channel *c, const UserInstance *ui)
{
	ASSERT(m_root, "setScope lack of a root instance.");

	UserCmdScopes *scope = (UserCmdScopes *)c->getContainers()->get(m_root);

	if (!scope) {
		scope = new UserCmdScopes();
		c->getContainers()->set(m_root, scope);
	}

	VERBOSE("set for " << ui->nickname);
	scope->cmds.insert({ ui, this });
}

void ChatCommand::resetScope(Channel *c, const UserInstance *ui)
{
	ASSERT(m_root, "setScope lack of a root instance.");

	UserCmdScopes *scope = (UserCmdScopes *)c->getContainers()->get(m_root);
	if (!scope)
		return;

	VERBOSE("remove " << (ui ? "user-specific" : "all"));

	if (ui)
		scope->cmds.erase(ui);

	// Remove all data or single
	if (!ui || scope->cmds.empty())
		c->getContainers()->remove(m_root);
	return;
}

bool ChatCommand::run(Channel *c, UserInstance *ui, std::string &msg, bool is_scope) const
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
		if (it->second->run(c, ui, subbed, true))
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
	if (m_module && m_action && !is_scope) {
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
