#include "client_tui.h"
#include "chatcommand.h"
#include "connection.h"
#include "channel.h"
#include "iimpl_basic.h"
#include "logger.h"
#include "module.h"
#include "settings.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string.h>

typedef UserIdBasic UserIdTUI;
typedef ChannelIdBasic ChannelIdTUI;



// ============ Helper module ============

class TUIhelper : public IModule {
public:
	TUIhelper(IClient *cli, ChatCommand &cmd) :
		m_client(cli)
	{
		cmd.add("help",   (ChatCommandAction)&TUIhelper::cmd_help, this);
		cmd.add("quit",   (ChatCommandAction)&TUIhelper::cmd_quit, this);
		cmd.add("exit",   (ChatCommandAction)&TUIhelper::cmd_quit, this);
		cmd.add("load",   (ChatCommandAction)&TUIhelper::cmd_load, this);
		cmd.add("reload", (ChatCommandAction)&TUIhelper::cmd_reload, this);
		cmd.add("step",   (ChatCommandAction)&TUIhelper::cmd_step, this);

		ChatCommand &c = cmd.add("channel", this);
		c.add("list",   (ChatCommandAction)&TUIhelper::cmd_channel_list, this);
		c.add("add",    (ChatCommandAction)&TUIhelper::cmd_channel_add, this);
		c.add("remove", (ChatCommandAction)&TUIhelper::cmd_channel_remove, this);
		ChatCommand &u = cmd.add("user", this);
		u.add("list",   (ChatCommandAction)&TUIhelper::cmd_user_list, this);
		u.add("add",    (ChatCommandAction)&TUIhelper::cmd_user_add, this);
		u.add("remove", (ChatCommandAction)&TUIhelper::cmd_user_remove, this);
		u.add("say",    (ChatCommandAction)&TUIhelper::cmd_user_say, this);
	}

	CHATCMD_FUNC(cmd_help)
	{
		sendRaw(
			"Available commands: \n"
			"\t quit / exit\n"
			"\t load <filename> (Execute commands from file)\n"
			"\t reload <modulename> [<keep_data>]\n"
			"\t step <seconds>\n"
			"\t channel list\n"
			"\t channel add    <name>\n"
			"\t channel remove <name>\n"
			"\t user list   [<channel>]\n"
			"\t user add    <name> <channel>\n"
			"\t user remove <name> <channel>\n"
			"\t user say    <name> <channel> <text>"
		);
	}

	CHATCMD_FUNC(cmd_quit)
	{
		((ClientTUI *)m_client)->m_is_alive = false;
	}

	CHATCMD_FUNC(cmd_load)
	{
		if (m_load_locked) {
			ERROR("Attempted to daisy-chain load commands");
			return;
		}
		m_load_locked = true;

		std::ifstream is(strtrim(msg));
		if (!is.good()) {
			sendRaw("File not found.");
			return;
		}
		std::string line;
		while (std::getline(is, line)) {
			if (line.empty() || line[0] == '#')
				continue; // Comment of empty

			((ClientTUI *)m_client)->m_commands->run(nullptr, nullptr, line);
		}
		m_load_locked = false;
	}

	CHATCMD_FUNC(cmd_reload)
	{
		std::string name(get_next_part(msg));
		bool keep_data = is_yes(msg);
		getModuleMgr()->reloadModule(name, keep_data);
	}

	CHATCMD_FUNC(cmd_step)
	{
		const char *pos = msg.c_str();
		float t = -1.0f;
		SettingType::parseFloat(&pos, &t);
		if (t <= 0.0f) {
			sendRaw("Step size must be greater than 0.0s");
			return;
		}
		getModuleMgr()->onStep(t);
	}

	CHATCMD_FUNC(cmd_channel_list)
	{
		const auto &all = getNetwork()->getAllChannels();
		char buf[BUFSIZE];
		std::ostringstream ss;
		ss << "List of channels: " << std::endl;
		for (Channel *c : all) {
			snprintf(buf, BUFSIZE, "\t[%s] : %zu users, %zu containers",
				c->cid->idStr().c_str(), c->getAllUsers().size(), c->getContainers()->size());
			ss << buf << std::endl;
		}
		sendRaw(ss.str());
	}

	CHATCMD_FUNC(cmd_channel_add)
	{
		std::string name(get_next_part(msg));
		if (name.empty()) {
			sendRaw("No name specified");
			return;
		}
		m_client->actionJoin(name);
	}

	CHATCMD_FUNC(cmd_channel_remove)
	{
		ChannelIdTUI cid(get_next_part(msg));
		Channel *c2 = getNetwork()->getChannel(cid);
		if (!c2) {
			sendRaw("Channel not found");
			cmd_channel_list(c, ui, msg);
			return;
		}
		m_client->actionLeave(c2);
	}

	CHATCMD_FUNC(cmd_user_list)
	{
		ChannelIdTUI cid(get_next_part(msg));
		IUserOwner *where = getNetwork();
		if (!cid.name.empty()) {
			where = getNetwork()->getChannel(cid);
			if (!where) {
				sendRaw("Channel not found");
				cmd_channel_list(c, ui, msg);
				return;
			}
		}
	
		const auto &all = where->getAllUsers();
		char buf[BUFSIZE];
		std::ostringstream ss;
		ss << "List of users: " << std::endl;
		for (UserInstance *ui : all) {
			snprintf(buf, BUFSIZE, "\t[%s] : %zu containers",
				ui->uid->nameStr().c_str(), ui->size());
			ss << buf << std::endl;
		}
		sendRaw(ss.str());
	}

	CHATCMD_FUNC(cmd_user_add)
	{
		std::string nick(get_next_part(msg));
		ChannelIdTUI cid(get_next_part(msg));
		c = getNetwork()->getChannel(cid);
		if (!c) {
			sendRaw("Channel not found");
			cmd_channel_list(c, ui, msg);
			return;
		}

		ui = c->addUser(UserIdTUI(nick));
		ui->nickname = nick;
		ui->account = UserInstance::UAS_LOGGED_IN;

		getModuleMgr()->onUserJoin(c, ui);
	}

	CHATCMD_FUNC(cmd_user_remove)
	{
		std::string nick(get_next_part(msg));
		ChannelIdTUI cid(get_next_part(msg));
		c = getNetwork()->getChannel(cid);
		if (!c) {
			sendRaw("Channel not found");
			cmd_channel_list(c, ui, msg);
			return;
		}
		ui = c->getUser(nick);
		if (!ui) {
			sendRaw("User not found");
			cmd_user_list(c, ui, msg);
			return;
		}
		getModuleMgr()->onUserLeave(c, ui);
		c->removeUser(ui);
	}

	CHATCMD_FUNC(cmd_user_say)
	{
		std::string nick(get_next_part(msg));
		ChannelIdTUI cid(get_next_part(msg));
		c = getNetwork()->getChannel(cid);
		if (!c) {
			sendRaw("Channel not found");
			cmd_channel_list(c, ui, msg);
			return;
		}
		ui = c->getUser(nick);
		if (!ui) {
			sendRaw("User not found");
			cmd_user_list(c, ui, msg);
			return;
		}
		msg = strtrim(msg);

		LoggerAssistant(LL_VERBOSE, nullptr) << "Channel [" << c->cid->nameStr() << "] " << ui->nickname << ": " << msg;
		if (!getModuleMgr()->onUserSay(c, ui, msg))
			sendRaw("Message was not maked as handled.");
	}

private:
	// Overlay IModule  shorthand functions because IModule::m_client is not available
	ModuleMgr *getModuleMgr() const { return m_client->getModuleMgr(); }
	Network *getNetwork() const { return m_client->getNetwork(); }
	void sendRaw(cstr_t &what) const { m_client->sendRaw(what); }

	const size_t BUFSIZE = 256;
	bool m_load_locked = false;
	IClient *m_client;
};


ClientTUI::ClientTUI(Settings *settings) :
	IClient(settings)
{
	m_commands = new ChatCommand(nullptr);
	m_helper = new TUIhelper(this, *m_commands);

	m_settings->set("client.admin", "ADMIN");
}

ClientTUI::~ClientTUI()
{
	m_is_alive = false;
	if (m_thread) {
		pthread_join(m_thread, nullptr);
		m_thread = 0;
	}

	delete m_helper;
}

void ClientTUI::initialize()
{
	// Capture command line inputs in a separate thread
	// to not block the event processing
	m_is_alive = true;
	int status = pthread_create(&m_thread, nullptr, &inputHandler, this);
	if (status != 0)
		ERROR("pthread failed: " << strerror(status));
}

bool ClientTUI::run()
{
	if (m_is_alive)
		m_module_mgr->onStep(-1);

	return m_is_alive;
}

void ClientTUI::sendRaw(cstr_t &text)
{
	std::cout << text << std::endl;
}

void ClientTUI::actionSay(Channel *c, cstr_t &text)
{
	LoggerAssistant(LL_NORMAL, nullptr) << "Channel [" << c->cid->nameStr() << "] Say: " << text;
}

void ClientTUI::actionReply(Channel *c, UserInstance *ui, cstr_t &text)
{
	LoggerAssistant(LL_NORMAL, nullptr) << "Channel [" << c->cid->nameStr() << "] Reply -> " << ui->uid->nameStr() << ": " << text;
}

void ClientTUI::actionNotice(Channel *c, UserInstance *ui, cstr_t &text)
{
	LoggerAssistant(LL_NORMAL, nullptr) << "Channel [" << c->cid->nameStr() << "] Notice -> " << ui->uid->nameStr() << ": " << text;
}

void ClientTUI::actionJoin(cstr_t &channel)
{
	LoggerAssistant(LL_NORMAL, nullptr) << "Channel [" << channel << "] Join";
	Channel *c = getNetwork()->addChannel(false, ChannelIdTUI(channel));
	m_module_mgr->onChannelJoin(c);
}

void ClientTUI::actionLeave(Channel *c)
{
	LoggerAssistant(LL_NORMAL, nullptr) << "Channel [" << c->cid->nameStr() << "] Leave";
	m_module_mgr->onChannelLeave(c);
	getNetwork()->removeChannel(c);
}


void *ClientTUI::inputHandler(void *cli_p)
{
	ClientTUI *cli = (ClientTUI *)cli_p;
	const size_t BUFSIZE = 512;
	char buf[BUFSIZE];

	while (cli->m_is_alive) {
		std::cout << "command:# " << std::flush;
		
		std::cin.getline(buf, BUFSIZE);

		if (strlen(buf) == 0)
			continue;

		if (!cli->m_commands->run(nullptr, nullptr, buf, true))
			std::cout << "Unknown command. Check 'help'." << std::endl;
	}

	return nullptr;
}

void ClientTUI::processRequest(ClientRequest &cr)
{
	switch (cr.type) {
		case ClientRequest::RT_NONE:
		case ClientRequest::RT_STATUS_UPDATE:
			return;
		case ClientRequest::RT_RELOAD_MODULE:
			m_module_mgr->reloadModule(
				*cr.reload_module.path,
				cr.reload_module.keep_data);
			return;
	}
}
