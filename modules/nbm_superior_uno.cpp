#include "../core/channel.h"
#include "../core/chatcommand.h"
#include "../core/module.h"
#include "../core/settings.h"
#include "../core/utils.h"
#include <algorithm> // std::sort
#include <cstring>
#include <queue>
#include <sstream>

struct Card {
	bool operator<(const Card &b)
	{
		if (color == b.color)
			return strcmp(face, b.face) > 0;
		return color > b.color;
	}

	static std::string format(const std::vector<Card> &cards)
	{
		std::ostringstream ss;
		ss << "\x0F"; // Normal text + Bold start
		for (const Card &c : cards)
			ss << colorize_string("\x02[" + std::string(c.face) + "] ", c.color);

		ss << "\x0F"; // Normal text
		return ss.str();
	}

	static size_t findType(cstr_t &type)
	{
		size_t i;
		for (i = 0; i < FACES_MAX; ++i) {
			if (type == FACES[i])
				break;
		}
		return i;
	}

	IRC_Color color = IC_BLACK;
	const char *face = nullptr;

	static const size_t FACES_MAX = 15;
	static const char *FACES[];
	static const size_t COLORS_MAX = 5;
	static const IRC_Color COLORS[];  // IRC colors
};

const char *Card::FACES[Card::FACES_MAX] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "D2", "R", "S", "W", "WD4"
};

const IRC_Color Card::COLORS[Card::COLORS_MAX] = { IC_BLACK, IC_RED, IC_GREEN, IC_BLUE, IC_YELLOW };

class UnoPlayer : public IContainer, public SettingType {
public:
	std::string dump() const { return "UnoPlayer"; }

	// For settings
	bool deSerialize(cstr_t &str)
	{
		const char *pos = str.c_str();

		return parseLong(&pos, &m_wins)
			&& parseLong(&pos, &m_losses)
			&& parseLong(&pos, &m_elo)
			&& parseLong(&pos, &m_streak);
	}

	// For settings
	std::string serialize() const
	{
		return std::to_string(m_wins)
			+ ' ' + std::to_string(m_losses)
			+ ' ' + std::to_string(m_elo)
			+ ' ' + std::to_string(m_streak);
	}

	std::vector<Card> drawCards(int count, int max_special = -1)
	{
		std::vector<Card> drawn;

		while (count > 0) {
			IRC_Color  color = Card::COLORS[get_random() % Card::COLORS_MAX];
			const char *face = Card::FACES[get_random() % Card::FACES_MAX];

			if (strchr(face, 'W')) {
				color = IC_BLACK; // must be black

				if (max_special == 0)
					continue; // Retry
				max_special--;
			} else if (color == IC_BLACK) {
				continue; // Retry
			}

			drawn.emplace_back(Card {
				.color = color,
				.face = face
			});
			count--;
		}

		cards.insert(cards.end(), drawn.begin(), drawn.end());
		std::sort(cards.begin(), cards.end());
		return drawn;
	}

	void win(std::set<UnoPlayer *> &players, size_t initial_player_count)
	{
		int elo_losers = 0;
		for (UnoPlayer *p : players) {
			if (p != this)
				elo_losers += p->m_elo;
		}
		int elo_all = elo_losers + m_elo;

		// Handle winner
		{
			double delta = (double)elo_losers / elo_all * GAIN_FACTOR + 0.5;
			m_delta += (int)delta;

			m_elo += (int)delta;
			if (initial_player_count == players.size()) {
				m_streak++;
				m_wins++;
			}
		}

		// Handle losers
		for (UnoPlayer *p : players) {
			if (p == this)
				continue;

			double delta = (double)p->m_elo / elo_all * -GAIN_FACTOR + 0.5;
			p->m_delta += (int)delta;

			p->m_elo += (int)delta;
			p->m_streak = 0;
			if (players.size() == MIN_PLAYERS) {
				// Only for last man standing
				p->m_losses++;
			}
		}
	}

	std::string formatElo(bool details)
	{
		std::ostringstream ss;
		if (details) {
			ss << m_elo << " Elo, "
				<< m_wins << " Wins / " << m_losses << " Losses, "
				<< m_streak << " Streak";
			return ss.str();
		}

		ss << m_elo << " (";
		ss << (m_delta > 0 ? "+" : "") << m_delta;
		if (m_streak > 1)
			ss << ", Streak " << m_streak;
		ss << ")";
		return ss.str();
	}

	long getElo() const
	{ return m_elo; }

	std::vector<Card> cards;
	static const size_t MIN_PLAYERS = true ? 2 : 1; // Debug mode
private:
	static const int GAIN_FACTOR = 20;
	long m_wins = 0,
		m_losses = 0,
		m_streak = 0,
		m_elo = 1000,
		m_delta = 0;
};

class UnoGame : public IContainer {
public:
	std::string dump() const { return "UnoGame"; }

	UnoGame(Channel *c, ChatCommand *cmd, Settings *s) :
		m_channel(c), m_commands(cmd), m_settings(s)
	{
		// 0x87
		modes = UM_RANKED | UM_UPGRADE | UM_STACK_WD4 | UM_STACK_D2;
	}

	~UnoGame()
	{
		for (UserInstance *ui : m_players) {
			m_commands->resetScope(m_channel, ui);
			ui->remove(this);
		}

		if (m_settings)
			m_settings->syncFileContents(SR_WRITE);
	}

	enum UnoMode : uint8_t
	{
		UM_STACK_D2  = 0x01, // Stack "draw +2" cards
		UM_STACK_WD4 = 0x02, // Stack "wild draw +4" cards
		UM_UPGRADE   = 0x04, // Place "wild draw +4" onto "draw +2"
		UM_LIGRETTO  = 0x40, // Smash in cards whenever you have a matching one
		UM_RANKED    = 0x80, // Ranked game, requires auth to join
	};

	bool checkMode(UnoMode mode)
	{ return (modes & (uint8_t)mode) != 0; }

	std::string dumpModes()
	{
		std::vector<const char *> out;
		if (checkMode(UM_STACK_D2))  out.push_back("Stack D2");
		if (checkMode(UM_STACK_WD4)) out.push_back("Stack WD4");
		if (checkMode(UM_UPGRADE))   out.push_back("Upgrade D2 -> WD4");
		if (checkMode(UM_LIGRETTO))  out.push_back("Ligretto");
		if (checkMode(UM_RANKED))    out.push_back("Ranked");

		std::string strout;
		for (const char *t : out) {
			if (!strout.empty())
				strout.append(", ");
			strout.append(t);
		}
		return strout;
	}

	void turnNext()
	{
		if (m_players.size() == 0) {
			current = nullptr;
			return;
		}

		auto it = m_players.find(current);
		if (dir_forwards) {
			if (++it == m_players.end())
				it = m_players.begin();
		} else {
			if (it == m_players.begin())
				it = m_players.end();
			it--;
		}
		current = *it;
	}

	bool addPlayer(UserInstance *ui)
	{
		if (has_started)
			return false;

		if (getPlayer(ui))
			return false;

		m_commands->setScope(m_channel, ui);

		UnoPlayer *p = new UnoPlayer();
		ui->set(this, p);
		m_settings->get(ui->nickname, p);

		m_players.insert(ui);
		return true;
	}

	inline UnoPlayer *getPlayer(UserInstance *ui)
	{
		return (UnoPlayer *)ui->get(this);
	}

	void removePlayer(UserInstance *ui)
	{
		UnoPlayer *player = getPlayer(ui);
		if (!player)
			return;

		if (ui == current)
			turnNext();

		if (has_started && player->cards.size() == 0) {
			std::string msg;
			msg.append(ui->nickname).append(" finished the game. gg!");
			if (checkMode(UM_RANKED)) {
				std::set<UnoPlayer *> players;
				for (UserInstance *ui : m_players)
					players.insert(getPlayer(ui));

				player->win(players, m_initial_player_count);

				// Update settings!
				for (UserInstance *up : m_players)
					m_settings->set(up->nickname, getPlayer(up));

				msg.append(" Elo: ").append(player->formatElo(false));
			}
			m_channel->say(msg);
		} else if (!checkMode(UM_RANKED) || !ui->get(&ui->account)) {
			// This is a bit hacky. Check whether the account status update
			// is still ongoing, and give no direct feedback in that case.
			m_channel->say(ui->nickname + " left this UNO game.");
		}

		m_commands->resetScope(m_channel, ui);
		ui->remove(this);
		m_players.erase(ui);
	}

	size_t getPlayerCount() const
	{ return m_players.size(); }

	bool start()
	{
		if (m_players.size() < UnoPlayer::MIN_PLAYERS || has_started)
			return false;

		for (UserInstance *ui : m_players)
			getPlayer(ui)->drawCards(11, 2);
	
		has_started = true;
		m_initial_player_count = m_players.size();
		current = *m_players.begin();
		UnoPlayer *p = getPlayer(current);
		top_card = p->cards[get_random() % p->cards.size()];

		turnNext();
		return true;
	}

	bool has_started = false;
	UserInstance *current = nullptr;
	Card top_card;
	int draw_count = 0;
	bool dir_forwards = true;
	uint8_t modes;

private:
	Channel *m_channel;
	ChatCommand *m_commands;
	Settings *m_settings;
	std::set<UserInstance *> m_players;

	size_t m_initial_player_count;
};

struct WaitingForAuth {
	UserInstance *ui;
	Channel *c;
	float timeout;
	std::string msg;
};

class nbm_superior_uno : public IModule {
public:
	~nbm_superior_uno()
	{
		if (m_settings)
			m_settings->syncFileContents(SR_WRITE);
	}

	void onClientReady()
	{
		m_settings = getModuleMgr()->getSettings(this);

		ChatCommand &uno = getModuleMgr()->getChatCommand()->add("$uno", this);
		uno.setMain((ChatCommandAction)&nbm_superior_uno::cmd_help);
		uno.add("join",  (ChatCommandAction)&nbm_superior_uno::cmd_join);
		uno.add("leave", (ChatCommandAction)&nbm_superior_uno::cmd_leave);
		uno.add("deal",  (ChatCommandAction)&nbm_superior_uno::cmd_deal);
		uno.add("top",   (ChatCommandAction)&nbm_superior_uno::cmd_top);
		uno.add("p",     (ChatCommandAction)&nbm_superior_uno::cmd_play);
		uno.add("d",     (ChatCommandAction)&nbm_superior_uno::cmd_draw);
		uno.add("elo",   (ChatCommandAction)&nbm_superior_uno::cmd_elo);
		uno.add("elotop", (ChatCommandAction)&nbm_superior_uno::cmd_elotop);
		m_commands = &uno;
	}

	void onStep(float time)
	{
		if (m_waiting_for_auth.empty())
			return;

		auto &it = m_waiting_for_auth.front();

		if (getNetwork()->contains(it.c) &&
				it.c->contains(it.ui)) {

			it.timeout += time;
	
			if (it.ui->account == UserInstance::UAS_LOGGED_IN) {
				it.ui->remove(&it.ui->account); // Auth request pending
				cmd_join(it.c, it.ui, it.msg);
				m_waiting_for_auth.pop();
				return;
			}

			if (it.timeout < 3)
				return;

			it.c->notice(it.ui, "Authentication check failed. Are you logged in?");
			it.ui->remove(&it.ui->account); // Auth request pending
		}

		m_waiting_for_auth.pop();
	}

	inline UnoGame *getGame(Channel *c)
	{
		return (UnoGame *)c->getContainers()->get(this);
	}

	void onChannelLeave(Channel *c)
	{
		c->getContainers()->remove(this);
	}

	void onUserLeave(Channel *c, UserInstance *ui)
	{
		if (UnoGame *g = getGame(c)) {
			UserInstance *old_current = g->current;
			g->removePlayer(ui);

			if (processGameUpdate(c)) {
				if (old_current != g->current)
					tellGameStatus(c);
			}
		}
	}

	// Returns whether the game is still active
	bool processGameUpdate(Channel *c)
	{
		UnoGame *g = getGame(c);
		if (!g)
			return false;
		if (g->getPlayerCount() >= UnoPlayer::MIN_PLAYERS)
			return true;

		if (g->has_started) {
			c->say("[UNO] Game ended");
		} else if (g->getPlayerCount() > 0) {
			// Not started yet. Wait for players.
			return false;
		}

		m_settings->syncFileContents();
		c->getContainers()->remove(this);
		return false;
	}

	void tellGameStatus(Channel *c, UserInstance *to_user = nullptr)
	{
		UnoGame *g = getGame(c);
		if (!g)
			return;

		// No specific player. Take current one
		if (to_user == nullptr)
			to_user = g->current;

		UnoPlayer *p = g->getPlayer(g->current);
	
		std::ostringstream ss;
		ss << "[UNO] " << g->current->nickname << " (" << p->cards.size() << " cards) - ";
		ss << "Top card: " << Card::format({ g->top_card });
		if (g->draw_count > 0)
			ss << "- draw count: "<< g->draw_count;

		c->say(ss.str());

		c->notice(to_user, "Your cards: " + Card::format(g->getPlayer(to_user)->cards));
	}

	CHATCMD_FUNC(cmd_help)
	{
		c->say("Available subcommands: " + m_commands->getList());
	}

	CHATCMD_FUNC(cmd_join)
	{
		UnoGame *g = getGame(c);
		if (g && g->has_started) {
			c->reply(ui, "Please wait for " + g->current->nickname + " to finish their game.");
			return;
		}

		bool is_new = (g == nullptr);
		if (!g) {
			g = new UnoGame(c, m_commands, m_settings);
			c->getContainers()->set(this, g);
		}

		if (!g->addPlayer(ui)) {
			c->notice(ui, "You already joined the game");
			return;
		}

		if (is_new) {
			std::string modes(get_next_part(msg));
			long val = g->modes;
			if (SettingType::parseLong(modes, &val, 16))
				g->modes = val; // Data loss
		}

		if (g->checkMode(UnoGame::UM_RANKED)) {
			if (ui->account == UserInstance::UAS_LOGGED_IN) {
				// good
			} else if (ui->account == UserInstance::UAS_UNKNOWN) {
				addClientRequest(ClientRequest {
					.type = ClientRequest::RT_STATUS_UPDATE,
					.status_update_nick = new std::string(ui->nickname)
				});
				m_waiting_for_auth.push(WaitingForAuth {
					.ui = ui,
					.c = c,
					.timeout = 0,
					.msg = msg
				});
				ui->set(&ui->account, new IContainer()); // Auth request pending
				onUserLeave(c, ui);
				return;
			} else {
				c->notice(ui, "You must be logged in to play ranked UNO");
				onUserLeave(c, ui);
				return;
			}
		}

		std::string text("[UNO] " + std::to_string(g->getPlayerCount()) +
			" player(s) are waiting for a new UNO game. Modes: ");
		text.append(g->dumpModes());
		c->say(text);
	}

	CHATCMD_FUNC(cmd_leave)
	{
		UnoGame *g = getGame(c);
		UnoPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!g || !p) {
			c->notice(ui, "There is nothing to leave...");
			return;
		}
		onUserLeave(c, ui);
	}

	CHATCMD_FUNC(cmd_deal)
	{
		UnoGame *g = getGame(c);
		UnoPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!p || g->has_started) {
			c->notice(ui, "There is no game to start.");
			return;
		}
		if (!g->start()) {
			c->say("Game start failed. Either there are < 2 players, "
				"or the game is already ongoing.");
			return;
		}
		tellGameStatus(c);
	}

	CHATCMD_FUNC(cmd_top)
	{
		UnoGame *g = getGame(c);
		UnoPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!p || !g->has_started) {
			c->notice(ui, "You are not part of an ongoing game.");
			return;
		}
		tellGameStatus(c, ui);
	}

	CHATCMD_FUNC(cmd_play)
	{
		UnoGame *g = getGame(c);
		UnoPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!p || !g->has_started) {
			c->notice(ui, "huh?? You don't have any cards.");
			return;
		}
		if (g->checkMode(UnoGame::UM_LIGRETTO))
			g->current = ui;

		if (g->current != ui) {
			c->notice(ui, "It is not your turn (current: " + g->current->nickname + ").");
			return;
		}
		std::string color_s(get_next_part(msg));
		IRC_Color color_e = IC_BLACK;
		std::string face_s(get_next_part(msg));

		if (color_s.size() >= 2) {
			// Example: $uno p rd2
			face_s = color_s.substr(1);
			color_s = color_s.substr(0, 1);
		}

		for (char &c : face_s)
			c = toupper(c);

		// Convert text colors to IRC Colors
		switch (toupper(color_s[0])) {
			case 'R': color_e = IC_RED;    break;
			case 'G': color_e = IC_GREEN;  break;
			case 'B': color_e = IC_BLUE;   break;
			case 'Y': color_e = IC_YELLOW; break;
		}
		bool change_face = face_s.find('W') != std::string::npos;
		size_t face_index = Card::findType(face_s);

		if (color_e == IC_BLACK || face_index == Card::FACES_MAX) {
			c->notice(ui, "Invalid input. Syntax: $uno p <color> <face>, $p <color><face>.");
			return;
		}

		// Check whether color of face matches
		if (color_e != g->top_card.color && face_s != g->top_card.face
				&& !change_face) {

			c->notice(ui, "This card cannot be played. Please check color and face.");
			return;
		}

		auto it = std::find_if(p->cards.begin(), p->cards.end(), [=] (auto &a) {
			return a.face == face_s && (change_face || a.color == color_e);
		});

		if (it == p->cards.end()) {
			c->notice(ui, "You don't have this card.");
			return;
		}

		if (g->draw_count > 0) {
			bool ok = false;
			if (face_s == "D2" &&
					face_s == g->top_card.face && // No downgrade
					g->checkMode(UnoGame::UM_STACK_D2))
				ok = true;
			else if (face_s == "WD4" &&
					face_s == g->top_card.face &&
					g->checkMode(UnoGame::UM_STACK_WD4))
				ok = true;
			else if (face_s == "WD4" &&
					strcmp(g->top_card.face, "D2") == 0 &&
					g->checkMode(UnoGame::UM_UPGRADE))
				ok = true;

			if (!ok) {
				c->notice(ui, "You cannot play this card due to the top card.");
				return;
			}
		}

		// All OK. Put the card on top
		g->top_card = Card {
			.color = color_e,
			.face = Card::FACES[face_index]
		};
		p->cards.erase(it);

		bool pending_autodraw = false;

		if (face_s == "D2") {
			g->draw_count += 2;
			pending_autodraw = !g->checkMode(UnoGame::UM_STACK_D2);
		} else if (face_s == "WD4") {
			g->draw_count += 4;
			pending_autodraw = !g->checkMode(UnoGame::UM_STACK_WD4);
		} else if (face_s == "R") {
			if (g->getPlayerCount() > 2)
				g->dir_forwards ^= true; // Toggle
			else
				g->turnNext(); // Acts as Skip for 2 players
		} else if (face_s == "S") {
			g->turnNext();
		}

		g->turnNext();

		// Player won, except when it's again their turn (last card = skip)
		if (p->cards.empty() && g->current != ui)
			g->removePlayer(ui);

		if (!processGameUpdate(c))
			return; // Game ended

		if (pending_autodraw)
			cmd_draw(c, ui, "");
		else
			tellGameStatus(c);
	}

	CHATCMD_FUNC(cmd_draw)
	{
		UnoGame *g = getGame(c);
		UnoPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!p || !g->has_started) {
			c->notice(ui, "You are not part of an ongoing game.");
			return;
		}
		if (g->checkMode(UnoGame::UM_LIGRETTO))
			g->current = ui;

		if (g->current != ui) {
			c->notice(ui, "It is not your turn (current: " + g->current->nickname + ").");
			return;
		}
		auto drawn = p->drawCards(std::max(1, g->draw_count));
		g->draw_count = 0;
		c->notice(ui, "You drew the following cards: " + Card::format(drawn));

		g->turnNext();
		tellGameStatus(c);
	}

	CHATCMD_FUNC(cmd_elo)
	{
		std::string nick(get_next_part(msg));
		if (nick.empty())
			nick = ui->nickname;

		UnoPlayer dummy;
		if (!m_settings->get(nick, &dummy)) {
			c->notice(ui, "There is no Elo record");
			return;
		}

		c->say("Selected player " + nick + ": " + dummy.formatElo(true));
	}

	CHATCMD_FUNC(cmd_elotop)
	{
		struct Score {
			const std::string *name;
			long score;
		};
		std::vector<Score> top;
		auto keys = m_settings->getKeys();
		UnoPlayer dummy;

		for (auto &k : keys) {
			if (!m_settings->get(k, &dummy))
				continue; // wrong format??

			top.push_back(Score {
				.name = &k,
				.score = dummy.getElo()
			});
		}
		std::sort(top.begin(), top.end(), [] (auto a, auto b) -> bool {
			return a.score > b.score;
		});

		std::ostringstream ss;
		for (size_t n = 0; n < 5 && n < top.size(); n++) {
			if (n > 0)
				ss << ", ";
			ss << *top[n].name << " (" << top[n].score << ")";
		}

		c->say("[UNO] Top 5 leaderboard: " + ss.str());
	}

private:
	std::queue<WaitingForAuth> m_waiting_for_auth;
	Settings *m_settings = nullptr;
	ChatCommand *m_commands = nullptr;
};


extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_superior_uno();
	}
}
