// Class for the protocol implementation

#include "client_irc.h"
#include "connection.h"
#include "channel.h"
#include "iimpl_basic.h"
#include "logger.h"
#include "module.h"
#include "settings.h"
#include <cstring>
#include <iostream>
#include <memory> // unique_ptr


// ============ User ID / Channel ID ============

struct UserIdIRC : UserIdBasic {
	UserIdIRC(cstr_t &nick) : UserIdBasic(nick) {}

	IImplId *copy(void *parent) const override
	{
		UserInstance *ui = (UserInstance *)parent;
		ui->nickname = *nickptr;
		return new UserIdIRC(ui->nickname);
	}

	std::string hostmask;
};

typedef ChannelIdBasic ChannelIdIRC;



// ============ Formatter ============

class FormatterIRC : public IFormatter {
public:
	void beginImpl(IRC_Color color)
	{
		*m_os << "\x03";
		if ((int)color < 10)
			*m_os << '0';
		*m_os << (int)color;
	}

	void beginImpl(FormatType flags)
	{
		endImpl(flags); // same
	}

	void endImpl(FormatType flags)
	{
		if (flags == FT_ALL) {
			*m_os << '\x0F';
			return;
		}
		if (flags & FT_BOLD)
			*m_os << '\x02';
		if (flags & FT_ITALICS)
			*m_os << '\x1D';
		if (flags & FT_UNDERL)
			*m_os << '\x1F';
		if (flags & FT_COLOR)
			*m_os << "\x03\u200B";
	}
};


// ============ Client class ============

struct ClientActionEntry {
	size_t offset;
	const char *status;
	void (ClientIRC::*handler)(cstr_t &status, NetworkEvent *e);
};

struct NetworkEvent {
	std::string nickname;
	std::string hostmask;
	std::string text;

	std::vector<std::string> args;

	void dump()
	{
		std::cout << "\tPacket dump: ";
		for (const auto &s : args)
			std::cout << s << " ";
		std::cout << ": " << text << std::endl;
	}
};

ClientIRC::ClientIRC(Settings *settings) :
	IClient(settings)
{
}

ClientIRC::~ClientIRC()
{
	if (m_con && m_auth_status != AS_SEND_NICK) {
		sendRaw("QUIT :Goodbye");
		SLEEP_MS(100);
	}

	delete m_con;
	m_con = nullptr;
}

void ClientIRC::initialize()
{
	m_nickname = m_settings->get("irc.nickname");
	SettingType::parseS64(m_settings->get("irc.authtype"), &m_auth_type);

	const std::string &addr = m_settings->get("irc.address");
	int64_t port = 0;
	SettingType::parseS64(m_settings->get("irc.port"), &port);

	m_con = Connection::createStream(addr, port);
	m_con->connect();
	sendRaw("PING server");
}

void ClientIRC::processRequest(ClientRequest &cr)
{
	VERBOSE("Got type " << (int)cr.type);

	switch (cr.type) {
		case ClientRequest::RT_NONE:
			return;
		case ClientRequest::RT_RELOAD_MODULE:
			m_module_mgr->reloadModule(
				*cr.reload_module.path,
				cr.reload_module.keep_data);
			return;
		case ClientRequest::RT_STATUS_UPDATE:
			if (!((IUserOwner *)m_network)->contains(cr.status_update))
				return;

			requestAccStatus(cr.status_update);
			return;
		//default: return;
	}
}

// TODO: change this to an event queue made from struct { enum, union }
void ClientIRC::sendRaw(cstr_t &text)
{
	MutexLock _(m_send_queue_lock);
	if (m_send_queue.size() >= SEND_QUEUE_MAX) {
		ERROR("Queue hard limit reached. Spam flood?");

		while (!m_send_queue.empty())
			m_send_queue.pop();
		return;
	}

	m_send_queue.push(text + '\n');
}

void ClientIRC::actionSay(Channel *c, cstr_t &text)
{
	sendRaw("PRIVMSG " + c->cid->idStr() + " :" + text);
}

void ClientIRC::actionReply(Channel *c, UserInstance *ui, cstr_t &text)
{
	actionSay(c, ui->uid->nameStr() + ": " + text);
}

void ClientIRC::actionNotice(Channel *c, UserInstance *ui, cstr_t &text)
{
	// Some IRC clients show this text in a separate tab... :(
	sendRaw("NOTICE " + ui->uid->idStr() + " :" + text);
}

void ClientIRC::actionJoin(cstr_t &channel)
{
	sendRaw("JOIN " + channel);
}

void ClientIRC::actionLeave(Channel *c)
{
	if (c->isPrivate())
		return;
	sendRaw("PART " + c->cid->idStr());
}

IFormatter *ClientIRC::createFormatter() const
{
	return new FormatterIRC();
}

bool ClientIRC::run()
{
	if (!m_con) {
		ERROR("Connection died");
		return false;
	}

	{
		// Process send queue
		MutexLock _(m_send_queue_lock);
		if (!m_send_queue.empty()) {
			m_con->send(m_send_queue.front());
			m_send_queue.pop();
		}
	}

	m_module_mgr->onStep(-1);

	// Process incoming lines, one-by-one
	std::unique_ptr<std::string> what(m_con->popRecv());

	if (!what)
		return true;

	g_logger->getFile(LL_NORMAL) << *what << std::endl;

	NetworkEvent e;

	// Extract regular text if available
	auto text_pos = what->find(':');
	{
		if (text_pos == std::string::npos)
			text_pos = what->size();
		else
			e.text = what->substr(text_pos + 1);
	}

	// Split by spaces into "e.args"
	{
		size_t pos = 0;
		while (pos < text_pos) {
			auto space_pos = what->find(' ', pos);
			if (space_pos == std::string::npos)
				space_pos = text_pos;
			if (space_pos > text_pos)
				break;

			// Only add non-empty entries
			if (space_pos > pos)
				e.args.emplace_back(what->substr(pos, space_pos - pos));

			pos = space_pos + 1;
		}
	}

	// Find matching action based on status code and index offset
	const ClientActionEntry *action = s_actions;
	while (true) {
		if (e.args.size() > action->offset) {
			if (e.args[action->offset] == action->status)
				break; // Found match!
		}

		// Try next entry
		action++;
		if (!action->status) {
			// last entry reached
			handleUnknown(*what);
			return true;
		}
	}

	if (action->offset == 1) {
		const std::string &arg = e.args[0];
		// Format: nickname!hostname status ...
		auto host_pos = arg.find('!');
		if (host_pos != std::string::npos) {
			e.nickname = arg.substr(0, host_pos);
			e.hostmask = arg.substr(host_pos + 1);
		} else {
			e.hostmask = arg;
		}
	}

	// Run the action if there is any to run
	if (action->handler)
		(this->*action->handler)(action->status, &e);

	// TODO: This is a bad location
	if (m_auth_status == AS_JOIN_CHANNELS)
		joinChannels();

	return true;
}

void ClientIRC::handleUnknown(cstr_t &msg)
{
	VERBOSE("Unknown: " << msg);
}

void ClientIRC::handleError(cstr_t &status, NetworkEvent *e)
{
	ERROR(e->text);
	delete m_con;
	m_con = nullptr;
}

void ClientIRC::handleClientEvent(cstr_t &status, NetworkEvent *e)
{
	VERBOSE("Client event: " << status);
	//e->dump();

	if (status == "JOIN") {
		// nick!host JOIN :channel
		ChannelIdIRC cid(e->text);
		Channel *c = m_network->getChannel(cid);
		if (!c) {
			c = m_network->addChannel(false, cid);
			// Add bot to channel
			c->addUser(UserIdIRC(m_nickname));
		}

		// Add newly joined user
		UserInstance *ui = c->addUser(UserIdIRC(e->nickname));
		((UserIdIRC *)ui->uid)->hostmask = e->hostmask;
		requestAccStatus(ui);

		if (e->nickname != m_nickname)
			m_module_mgr->onUserJoin(c, ui);
	}
	if (status == "MODE") {
		cstr_t &what = e->args[2];
		cstr_t &modes = e->text;

		if (what[0] == '#') {
			// Channel mode
			return;
		}

		UserInstance *ui = m_network->getUser(what);
		if (!ui) {
			WARN("MODE not implemented for " << what);
			return;
		}

		if (what == m_nickname) {
			// Add and remove user modes
			apply_user_modes(m_user_modes, modes);

			// Our mode updated. Auth, if not already done.
			if (m_auth_status == AS_AUTHENTICATE) {
				if (m_auth_type > 0)
					sendRaw("PRIVMSG NickServ :identify " + m_settings->get("irc.password"));
			}

			if (strchr(m_user_modes, 'r'))
				m_auth_status = AS_JOIN_CHANNELS;
			if (m_auth_type == 0 && strchr(m_user_modes, 'i'))
				m_auth_status = AS_JOIN_CHANNELS;
		}
		return;
	}
	if (status == "NICK") {
		// nick!host NICK :NewNickname
		UserInstance *ui = m_network->getUser(UserIdIRC(e->nickname));
		ui->nickname = e->text;
		requestAccStatus(ui);

		m_module_mgr->onUserRename(ui, e->nickname);
		return;
	}
	if (status == "PART" || status == "KICK") {
		// nick!host PART channel :reason
		ChannelIdIRC cid(e->args[2]);
		Channel *c = m_network->getChannel(cid);
		UserInstance *ui = c->getUser(e->nickname);
		if (!c || !ui) {
			ERROR("Invalid channel or user");
			return;
		}

		if (e->nickname == m_nickname) {
			m_module_mgr->onChannelLeave(c);
			m_network->removeChannel(c);
			return;
		}

		m_module_mgr->onUserLeave(c, ui);
		c->removeUser(ui);
		return;
	}
	if (status == "QUIT") {
		if (e->nickname == m_nickname) {
			for (Channel *c : m_network->getAllChannels()) {
				m_module_mgr->onChannelLeave(c);
			}
			//delete this; // eeh.. maybe?
			delete m_network;
			m_network = nullptr;
			return;
		}

		// Remove the user from all channels
		UserInstance *ui = m_network->getUser(e->nickname);
		for (Channel *c : m_network->getAllChannels()) {
			if (c->removeUser(ui)) {
				m_module_mgr->onUserLeave(c, ui);
			}
		}
		// .. and from the entire network to drop the instance
		m_network->removeUser(ui);
		ui = nullptr;
		return;
	}
	if (status == "INVITE") {
		sendRaw("JOIN " + e->text);
		return;
	}
}

void ClientIRC::handleChatMessage(cstr_t &status, NetworkEvent *e)
{
	{
		// Log this line
		auto &os = g_logger->getStdout(LL_NORMAL);
		write_timestamp(&os);
		os << " ";
		if (!e->nickname.empty() && e->nickname != m_nickname)
			os << e->nickname << " \t";

		os << e->text << std::endl;
	}

	if (status == "NOTICE") {
		if (e->nickname != "NickServ")
			return;

		// Retrieve pending user status requests
		// STATUS username int
		auto args = strsplit(e->text);
		if (args.size() != 3)
			return;

		int status = -1;
		std::string *nick = nullptr;

		if (m_auth_type == 1) {
			if (args[1] == "ACC") {
				// For IRCd: solanum
				status = args[2][0] - '0';
				nick = &args[0];
			}
		} else if (m_auth_type == 2) {
			if (args[0] == "STATUS") {
				// For IRCd: plexus
				status = args[2][0] - '0';
				nick = &args[1];
			}
		}
		VERBOSE(args[0] << "|" << args[1] << " -> " << status);

		UserInstance *ui = nick ? m_network->getUser(*nick) : nullptr;
		if (ui) {
			ui->account = static_cast<UserInstance::UserAccStatus>(status);
			m_module_mgr->onUserStatusUpdate(ui, false);
		}
		return;
	}
	if (status == "PRIVMSG") {
		// nick!host PRIVMSG where :text text
		cstr_t &channel = e->args[2];
		if (channel[0] != '#') {
			WARN("Private message handling is yet not implemented.");
			return;
		}

		ChannelIdIRC cid(channel);
		Channel *c = m_network->getChannel(cid);
		if (!c) {
			ERROR("Got PRIVMSG before JOIN:  " << cid.nameStr());
			c = m_network->addChannel(false, cid);
		}

		m_module_mgr->onUserSay(c, m_network->getUser(e->nickname), e->text);
		return;
	}
}

void ClientIRC::handlePing(cstr_t &status, NetworkEvent *e)
{
	//VERBOSE("PONG!");
	sendRaw("PONG " + e->text);
}

void ClientIRC::handleAuthentication(cstr_t &status, NetworkEvent *e)
{
	if (m_auth_status == AS_SEND_NICK) {
		LOG("Auth with nick: " << m_nickname);

		sendRaw("USER " + m_nickname + " foo bar :Generic description");
		sendRaw("NICK " + m_nickname);

		int64_t type = 0;
		SettingType::parseS64(m_settings->get("irc.authtype"), &type);
		if (type > 0)
			m_auth_status = AS_AUTHENTICATE;

		m_network->addUser(UserIdIRC(m_nickname));
		return;
	}
	if (status == "396") {
		// Hostmask changed. This indicates successful authentication
		m_auth_status = AS_JOIN_CHANNELS;
		return;
	}
}

void ClientIRC::handleServerMessage(cstr_t &status, NetworkEvent *e)
{
	if (status == "353") {
		// User list
		ChannelIdIRC cid(e->args[4]);
		std::vector<std::string> users = strsplit(e->text);

		Channel *c = m_network->addChannel(false, cid);
		for (auto &name : users) {
			// Trim first character if necessary
			if (strchr("~&@%+", name[0]))
				name = name.substr(1);

			c->addUser(UserIdIRC(name));
		}
		m_module_mgr->onChannelJoin(c);
		return;
	}
}

void ClientIRC::joinChannels()
{
	if (m_auth_status != AS_JOIN_CHANNELS)
		return;
	m_auth_status = AS_DONE;

	auto channels = strsplit(m_settings->get("irc.channels"));
	for (const std::string &chan : channels) {
		if (chan.size() < 2 || chan[0] != '#')
			continue;

		sendRaw("JOIN " + chan);
	}
}

void ClientIRC::requestAccStatus(UserInstance *ui)
{
	if (ui->account == UserInstance::UserAccStatus::UAS_PENDING)
		return;

	ui->account = UserInstance::UserAccStatus::UAS_PENDING;
	std::string text("PRIVMSG NickServ :");
	if (m_auth_type == 1)
		text.append("ACC ");
	else
		text.append("STATUS ");

	sendRaw(text + ui->nickname);
}

const ClientActionEntry ClientIRC::s_actions[] = {
	{ 0, "ERROR", &ClientIRC::handleError },
// Init messages and auth
	{ 1, "001", &ClientIRC::handleAuthentication },
	{ 1, "002", &ClientIRC::handleAuthentication },
	{ 1, "003", &ClientIRC::handleAuthentication },
	{ 1, "396", &ClientIRC::handleAuthentication }, // Hostmask changed
	{ 1, "439", &ClientIRC::handleAuthentication },
	{ 1, "451", &ClientIRC::handleAuthentication }, // ERR_NOTREGISTERED
// Server information and events
	{ 0, "PING", &ClientIRC::handlePing },
	{ 1, "250", &ClientIRC::handleChatMessage }, // User stats
	{ 1, "251", &ClientIRC::handleChatMessage }, // RPL_LUSERCLIENT
	{ 1, "253", &ClientIRC::handleChatMessage }, // RPL_LUSERUNKNOWN
	{ 1, "255", &ClientIRC::handleChatMessage }, // RPL_LUSERME
	{ 1, "332", &ClientIRC::handleChatMessage }, // RPL_TOPIC
	{ 1, "372", &ClientIRC::handleChatMessage }, // RPL_MOTD
	{ 1, "375", &ClientIRC::handleChatMessage }, // RPL_MOTDSTART
	{ 1, "376", &ClientIRC::handleChatMessage }, // RPL_ENDOFMOTD
	{ 1, "353", &ClientIRC::handleServerMessage },  // RPL_NAMREPLY (user list)
	{ 1, "366", &ClientIRC::handleChatMessage },  // RPL_ENDOFNAMES
// ClientIRC/user events
	{ 1, "JOIN", &ClientIRC::handleClientEvent },
	{ 1, "NICK", &ClientIRC::handleClientEvent },
	{ 1, "KICK", &ClientIRC::handleClientEvent },
	{ 1, "PART", &ClientIRC::handleClientEvent },
	{ 0, "QUIT", &ClientIRC::handleClientEvent },
	{ 1, "MODE",  &ClientIRC::handleClientEvent },
	{ 1, "PRIVMSG", &ClientIRC::handleChatMessage },
	{ 1, "NOTICE",  &ClientIRC::handleChatMessage },
	{ 1, "INVITE", &ClientIRC::handleClientEvent },
// Ignore
	{ 1, "004", nullptr },
	{ 1, "005", nullptr },
	{ 1, "252", nullptr },
	{ 1, "254", nullptr },
	{ 1, "265", nullptr },
	{ 1, "266", nullptr },
	{ 1, "333", nullptr },
	{ 0, nullptr, nullptr }, // Termination
};
