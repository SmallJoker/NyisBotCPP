#include "../core/channel.h"
#include "../core/chatcommand.h"
#include "../core/module.h"
#include "../core/settings.h"
#include "../core/utils.h"
#include <algorithm> // std::sort
#include <cstring>
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
		ss << "\x0F\x02"; // Normal text + Bold start
		for (const Card &c : cards)
			ss << colorize_string("[" + std::string(c.face) + "] ", c.color);

		ss << "\x0F "; // Normal text
		return ss.str();
	}

	IRC_Color color = IC_BLACK;
	const char *face = nullptr;

	static const size_t TYPES_MAX = 15;
	static const char *TYPES[];
	static const size_t COLORS_MAX = 5;
	static const IRC_Color COLORS[];  // IRC colors
};

const char *Card::TYPES[Card::TYPES_MAX] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "D2", "R", "S", "W", "WD4"
};

const IRC_Color Card::COLORS[Card::COLORS_MAX] = { IC_BLACK, IC_RED, IC_GREEN, IC_BLUE, IC_YELLOW };

class UnoPlayer : public IContainer, public SettingType {
public:
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
			const char *face = Card::TYPES[get_random() % Card::TYPES_MAX];

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
		}

		cards.insert(cards.end(), drawn.begin(), drawn.end());
		sortCards();
		return drawn;
	}

	void win(std::set<UnoPlayer *> &players, int initial_player_count)
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
			if (players.size() == 2) {
				// Only for last man standing
				p->m_losses++;
			}
		}
	}

	std::string formatElo(bool details)
	{
		return "stub";
	}

	void sortCards()
	{
		std::sort(cards.begin(), cards.end());
	}

	std::vector<Card> cards;
private:
	static const int GAIN_FACTOR = 30;
	long m_wins = 0,
		m_losses = 0,
		m_streak = 0,
		m_elo = 1000,
		m_delta = 0;
};

class UnoGame : public IContainer {
public:
	UnoGame(Channel *c, Settings *s) :
		m_channel(c), m_settings(s)
	{
		modes = UM_RANKED | UM_UPGRADE | UM_STACK_WD4 | UM_STACK_D2;
	}

	~UnoGame()
	{
		for (UserInstance *ui : m_players)
			ui->remove(this);

		if (m_settings)
			m_settings->syncFileContents();
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
		if (++it == m_players.end())
			it = m_players.begin();
		current = *it;
	}

	bool addPlayer(UserInstance *ui)
	{
		if (has_started)
			return false;

		if (getPlayer(ui))
			return false;

		ui->set(this, new UnoPlayer());
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
		} else {
			m_channel->say(ui->nickname + " left this UNO game.");
		}

		ui->remove(this);
		m_players.erase(ui);
	}

	size_t getPlayerCount() const
	{ return m_players.size(); }

	bool start()
	{
		if (m_players.size() < 2)
			return false;

		has_started = true;
		m_initial_player_count = m_players.size();
		current = *m_players.begin();
		return true;
	}

	bool has_started = false;
	UserInstance *current = nullptr;
	Card top_card;
	int draw_count = 0;
	uint8_t modes;

private:
	Channel *m_channel;
	Settings *m_settings;
	std::set<UserInstance *> m_players;

	int m_initial_player_count;
};

class nbm_superior_uno : public IModule {
public:
	~nbm_superior_uno()
	{
		if (m_settings)
			m_settings->syncFileContents();
		delete m_settings;
	}

	void initCommands(ChatCommand &cmd)
	{
		ChatCommand &uno = cmd.add("$uno", this);
		uno.add("join",  (ChatCommandAction)&nbm_superior_uno::cmd_join);
		uno.add("leave", (ChatCommandAction)&nbm_superior_uno::cmd_leave);
		uno.add("p",     (ChatCommandAction)&nbm_superior_uno::cmd_play);
		uno.add("d",     (ChatCommandAction)&nbm_superior_uno::cmd_draw);
	}

	void onClientReady()
	{
		if (m_settings)
			return;

		m_settings = getModuleMgr()->getSettings(this);
		m_settings->syncFileContents();
	}

	inline UnoGame *getGame(Channel *c)
	{
		return (UnoGame *)c->getContainers()->get(this);
	}

	void onUserLeave(Channel *c, UserInstance *ui)
	{
		if (UnoGame *g = getGame(c)) {
			UserInstance *old_current = g->current;
			g->removePlayer(ui);

			if (processGameUpdate(c)) {
				c->say("[UNO] " + ui->nickname + " left this game.");
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
		if (g->getPlayerCount() > 1)
			return true;
		if (g->has_started)
			c->say("[UNO] Game ended");

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

	CHATCMD_FUNC(cmd_join)
	{
		UnoGame *g = getGame(c);
		if (g && g->has_started) {
			c->reply(ui, "Please wait for " + g->current->nickname + " to finish their game.");
			return;
		}

		bool is_new = (g == nullptr);
		if (!g) {
			g = new UnoGame(c, m_settings);
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

		if (g->checkMode(UnoGame::UM_RANKED) &&
				ui->account != UserInstance::UAS_LOGGED_IN) {
			c->notice(ui, "You must be logged in to play ranked UNO");
			g->removePlayer(ui);
			return;
		}

		// TODO: Subcommand shortcut

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
			c->notice(ui, "You are not part of an UNO game.");
			return;
		}
		onUserLeave(c, ui);
	}

	CHATCMD_FUNC(cmd_play)
	{
		
	}

	CHATCMD_FUNC(cmd_draw)
	{
		
	}

private:
	Settings *m_settings = nullptr;
};


extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_superior_uno();
	}
}
