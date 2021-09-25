#include "../core/channel.h"
#include "../core/chatcommand.h"
#include "../core/module.h"
#include "../core/settings.h"
#include "../core/utils.h"
#include <algorithm> // std::sort
#include <iostream>
#include <random>
#include <sstream>

// Per-channel-member
struct LPlayer : public IContainer {
	std::string dump() const { return "LPlayer"; }

	static std::string format(const std::vector<const char *> &cards, bool hand_info = false)
	{
		std::ostringstream ss;
		if (hand_info)
			ss << "Your cards: ";

		ss << "\x0F"; // Normal text
		for (size_t i = 0; i < cards.size(); ++i) {
			if (hand_info)
				ss << (i + 1);

			std::string inner("[" + std::string(cards[i]) + "]");
			if (*(cards[i]) == 'Q') // Red queens
				ss << colorize_string(inner, IC_RED);
			else // All other cards
				ss << inner;

			if (i + 1 < cards.size())
				ss << ' ';
		}

		ss << "\x0F"; // Normal text
		return ss.str();
	}

	static void shuffle(std::vector<const char *> &cards)
	{
		std::random_shuffle(cards.begin(), cards.end(), [] (int i) -> int {
			return std::rand() % i;
		});
	}

	std::vector<const char *> cards;

	static const char *FACES[];
};

const char *LPlayer::FACES[] = {
	"6", "7", "8", "9", "10", "J", "Q", "K", "A"
};

// Per-channel
class LGame : public IContainer {
public:
	std::string dump() const { return "LGame"; }

	LGame(Channel *c, ChatCommand *cmd) :
		m_channel(c), m_commands(cmd) {}

	~LGame()
	{
		for (UserInstance *ui : m_players) {
			m_commands->resetScope(m_channel, ui);
			ui->remove(this);
		}
	}

	inline LPlayer *getPlayer(UserInstance *ui)
	{
		return (LPlayer *)ui->get(this);
	}

	bool addPlayer(UserInstance *ui)
	{
		if (has_started || getPlayer(ui))
			return false;

		m_commands->setScope(m_channel, ui);

		LPlayer *p = new LPlayer();
		ui->set(this, p);
		m_players.insert(ui);
		return true;
	}

	void removePlayer(UserInstance *ui)
	{
		LPlayer *p = getPlayer(ui);
		if (!p)
			return;

		m_commands->resetScope(m_channel, ui);
		ui->remove(this);
		m_players.erase(ui);

		if (ui == current)
			turnNext();
	}

	UserInstance *getPrevPlayer()
	{
		if (m_players.size() == 0)
			return nullptr;

		auto it = m_players.find(current);
		if (it == m_players.begin())
			it = m_players.end();
		it--;
		return *it;
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

	const std::set<UserInstance *> &getAllPlayers() const
	{ return m_players; }

	bool start()
	{
		if (m_players.size() < LGame::MIN_PLAYERS || has_started)
			return false;

		// List of all cards in the game
		std::vector<const char *> all_cards;
		all_cards.reserve(sizeof(LPlayer::FACES) / sizeof(char *));

		for (const char *c : LPlayer::FACES) {
			all_cards.emplace_back(c);
			all_cards.emplace_back(c);
			all_cards.emplace_back(c);
			if (*c != 'Q')
				all_cards.emplace_back(c);
		}
		LPlayer::shuffle(all_cards);

		// Distribute the cards to each player
		auto c_it = all_cards.begin();
		auto p_it = m_players.begin();
		while (c_it != all_cards.end()) {
			getPlayer(*p_it)->cards.emplace_back(*c_it);

			if (++p_it == m_players.end())
				p_it = m_players.begin();
			c_it++;
		}

		has_started = true;
		current = *m_players.begin();
		return true;
	}

	static const size_t MIN_PLAYERS = 3;

	bool has_started = false;
	UserInstance *current = nullptr;
	std::vector<const char *> stack;
	const char *main_face = nullptr;
	size_t stack_last = 0;

private:
	std::set<UserInstance *> m_players;
	Channel *m_channel;
	ChatCommand *m_commands;
};

class nbm_liar_game : public IModule {
public:
	nbm_liar_game()
	{
	}

	void initCommands(ChatCommand &cmd)
	{
		ChatCommand &lcmd = cmd.add("$lgame", this);
		lcmd.setMain((ChatCommandAction)&nbm_liar_game::cmd_help);
		lcmd.add("join",  (ChatCommandAction)&nbm_liar_game::cmd_join, this);
		lcmd.add("leave", (ChatCommandAction)&nbm_liar_game::cmd_leave, this);
		lcmd.add("start", (ChatCommandAction)&nbm_liar_game::cmd_start, this);
		lcmd.add("add",   (ChatCommandAction)&nbm_liar_game::cmd_add, this);
		lcmd.add("check", (ChatCommandAction)&nbm_liar_game::cmd_check, this);
		lcmd.add("cards", (ChatCommandAction)&nbm_liar_game::cmd_cards, this);
		m_commands = &lcmd;
	}

	inline LGame *getGame(Channel *c)
	{
		return (LGame *)c->getContainers()->get(this);
	}

	void onUserLeave(Channel *c, UserInstance *ui)
	{
		LGame *g = getGame(c);
		if (!g || !g->getPlayer(ui))
			return;

		g->removePlayer(ui);
		if (processGameUpdate(c)) {
			tellGameStatus(c);
		}
	}

	void onChannelLeave(Channel *c)
	{
		c->getContainers()->remove(this);
	}

	void tellGameStatus(Channel *c)
	{
		LGame *g = getGame(c);
		if (!g->has_started || g->stack.empty())
			return;

		UserInstance *ui = g->current;
		if (!ui)
			return;

		// Normal ongoing game
		std::ostringstream ss;

		ss << "[LGame] Main card: " << LPlayer::format({ g->main_face });
		ss << ", Stack height: " << g->stack.size();
		ss << ". Current player: " << ui->nickname;

		c->say(ss.str());
		c->notice(ui, LPlayer::format(g->getPlayer(ui)->cards, true));
	}

	// Returns true if the player finished
	bool updateCardsWin(Channel *c, UserInstance *ui, bool played_card)
	{
		LGame *g = getGame(c);
		if (!g || !g->has_started)
			return false;

		LPlayer *p = g->getPlayer(ui);
		if (!p)
			return false;

		size_t amount = p->cards.size();
		if (amount == 0) {
			// Done!
			if (ui == g->current) {
				c->say(ui->nickname + " has no cards left. Congratulations, you're a winner!");
				return true;
			} else if (played_card) {
				c->say(ui->nickname + " played " + colorize_string("their last card", IC_ORANGE) + "!");
			}
			return false;
		}
		if (amount >= 4) {
			// Sum all face types for discarding
			std::map<const char *, int> cards;
			for (const char *c : p->cards) {
				if (cards.find(c) != cards.end())
					cards[c]++;
				else
					cards[c] = 1;
			}

			// Discard if all four were collected
			std::vector<const char *> discarded;
			for (auto &card : cards) {
				if (card.second >= 4) {
					p->cards.erase(std::remove_if(p->cards.begin(), p->cards.end(),
							[card] (const char *d) -> bool {
						return d == card.first;
					}), p->cards.end());
					discarded.emplace_back(card.first);
				}
			}
			if (!discarded.empty()) {
				c->say(ui->nickname + " can discard four " + LPlayer::format(discarded) +
					" cards. Left cards: " + std::to_string(p->cards.size()));

				// Re-check for win
				return updateCardsWin(c, ui, false);
			}
			return false;
		}

		if (amount <= LGame::MIN_PLAYERS && ui == g->current) {
			c->say(ui->nickname + " has " +
				colorize_string("only " + std::to_string(amount) + " cards", IC_ORANGE) + " left!");
		}
		return false;
	}

	bool processGameUpdate(Channel *c)
	{
		LGame *g = getGame(c);
		if (!g)
			return false;
		if (g->getAllPlayers().size() >= LGame::MIN_PLAYERS)
			return true;

		if (g->has_started) {
			c->say("[LGame] Game ended");
		} else if (!g->getAllPlayers().empty()) {
			// Not started yet. Wait for players.
			return false;
		}

		c->getContainers()->remove(this);
		return false;
	}

	CHATCMD_FUNC(cmd_help)
	{
		c->say("Available subcommands: " + m_commands->getList());
	}

	CHATCMD_FUNC(cmd_join)
	{
		LGame *g = getGame(c);
		if (g && g->has_started) {
			c->reply(ui, "Please wait for " + g->current->nickname + " to finish their game.");
			return;
		}

		if (!g) {
			g = new LGame(c, m_commands);
			c->getContainers()->set(this, g);
		}

		if (!g->addPlayer(ui)) {
			c->reply(ui, "Yes yes I know that you're waiting.");
			return;
		}

		size_t count = g->getAllPlayers().size();
		std::ostringstream ss;
		ss << "Player " << ui->nickname << " joined the game. Total " << count << " players ready.";
		if (count >= LGame::MIN_PLAYERS)
			ss << " If you want to start the game, use \"$start\"";
		else if (count == 1)
			ss << " At least " << LGame::MIN_PLAYERS << " players are required to start the game.";

		c->say(ss.str());
	}

	CHATCMD_FUNC(cmd_leave)
	{
		LGame *g = getGame(c);
		LPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!p || !g->has_started) {
			c->notice(ui, "There is no game for you to leave");
			return;
		}

		onUserLeave(c, ui);
	}

	CHATCMD_FUNC(cmd_start)
	{
		LGame *g = getGame(c);
		LPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!p || g->has_started) {
			c->notice(ui, "There is no game for you to start");
			return;
		}

		if (!g->start()) {
			c->say("[LGame] Game start failed. There must be at least "
				+ std::to_string(LGame::MIN_PLAYERS) + " players waiting.");
			return;
		}

		ui = g->current;
		c->say("Game started! Player " + ui->nickname +
			" may play the first card using \"$add <'main card'> <card nr.> [<card nr.> [<card nr.>]]\"" +
			" (Card nr. from your hand)");

		for (UserInstance *ui : g->getAllPlayers())
			updateCardsWin(c, ui, false);

		c->notice(ui, LPlayer::format(g->getPlayer(ui)->cards, true));
	}

	CHATCMD_FUNC(cmd_add)
	{
		LGame *g = getGame(c);
		LPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!p || !g->has_started) {
			c->notice(ui, "You are not part of an ongoing game");
			return;
		}

		if (ui != g->current) {
			c->notice(ui, "It is yet not your turn. Current: " + g->current->nickname);
			return;
		}

		std::string face(get_next_part(msg));
		for (char &c : face)
			c = toupper(c);

		const char *face_c = nullptr;
		for (const char *card : LPlayer::FACES) {
			if (*card != 'Q' && face == card) {
				face_c = card;
				break;
			}
		}

		// Empty stack. Specify type
		if (!face_c) {
			std::string ret = "Unknown face. Must be one of the following: ";
			for (const char *card : LPlayer::FACES) {
				if (*card != 'Q') {
					ret.append(card);
					ret.append(" ");
				}
			}
			c->notice(ui, ret);
			return;
		}

		if (g->stack.empty()) {
			g->main_face = face_c;
		} else {
			// Validate
			if (face_c != g->main_face) {
				c->notice(ui, "Wrong card type! Please pretend to place a card of type " +
					LPlayer::format({ g->main_face }));
				return;
			}
		}

		// Face is specified. Add cards.
		std::set<size_t> indices;
		while (!msg.empty()) {
			std::string index(get_next_part(msg));
			long out = -1;
			if (!SettingType::parseLong(index, &out) ||
					out > (long)p->cards.size() || out < 1) {
				c->notice(ui, "Invalid card index \"" + std::to_string(out) +
					"\". Play one between 1 and " +
					std::to_string(p->cards.size()) + " from your hand.");
				return;
			}
			indices.insert(out - 1);
		}
		if (indices.empty()) {
			c->notice(ui, "Please specify at least one card index.");
			return;
		}

		// "indices" is sorted ascending
		// Remove in reverse order
		for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
			auto c_it = p->cards.begin() + *it;
			g->stack.emplace_back(*c_it);
			p->cards.erase(c_it);
		}
		g->stack_last = indices.size();

		// Cards added. Update game progress
		if (updateCardsWin(c, ui, true))
			g->removePlayer(ui);
		else
			g->turnNext();

		if (!processGameUpdate(c))
			return; // Game ended

		tellGameStatus(c);
	}

	CHATCMD_FUNC(cmd_check)
	{
		LGame *g = getGame(c);
		LPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!p || !g->has_started) {
			c->notice(ui, "You are not part of an ongoing game");
			return;
		}

		if (ui != g->current) {
			c->notice(ui, "It is yet not your turn. Current: " + g->current->nickname);
			return;
		}

		if (g->stack.empty()) {
			c->notice(ui, "There is no stack to check. Please start a new one.");
			return;
		}

		bool contains_invalid = false;
		std::vector<const char *> top_cards(g->stack.rbegin(), g->stack.rbegin() + g->stack_last);
		for (const char *card : top_cards) {
			if (card != g->main_face) {
				contains_invalid = true;
				break;
			}
		}

		std::ostringstream ss;
		if (contains_invalid) {
			ss << "One or more top cards were not a [" << g->main_face << "].";
		} else {
			ss << "The top cards were correct!";

			g->turnNext();
		}

		ss << " (" << LPlayer::format(top_cards) << ") ";

		// Previous player draws the cards
		UserInstance *ui_prev = g->getPrevPlayer();
		LPlayer *p_prev = g->getPlayer(ui_prev);
		p_prev->cards.insert(p_prev->cards.end(), g->stack.begin(), g->stack.end());

		g->stack.clear();
		g->stack_last = 0;
		g->main_face = nullptr;

		ss << "Complete stack goes to " << ui_prev->nickname << ". ";

		ss << g->current->nickname + " may start with an empty stack.";
		c->say(ss.str());

		// Show newest game status
		size_t num_players = g->getAllPlayers().size();
		ui = g->current;
		p = g->getPlayer(ui);

		if (updateCardsWin(c, ui_prev, false)) {
			// User played their last card and it was correct
			g->removePlayer(ui_prev);
		} else {
			c->notice(ui_prev, LPlayer::format(p_prev->cards, true));
		}
		if (updateCardsWin(c, ui, false)) {
			// Obscure win: discard all 4 in the hand
			g->removePlayer(ui);
		} else {
			c->notice(ui, LPlayer::format(p->cards, true));
		}

		if (!processGameUpdate(c))
			return;

		if (g->getAllPlayers().size() != num_players) {
			tellGameStatus(c);
		}
	}

	CHATCMD_FUNC(cmd_cards)
	{
		LGame *g = getGame(c);
		LPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!p || !g->has_started) {
			c->notice(ui, "You are not part of an ongoing game");
			return;
		}

		p->shuffle(p->cards);
		c->notice(ui, LPlayer::format(p->cards, true));
	}

private:
	ChatCommand *m_commands = nullptr;
};


extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_liar_game();
	}
}
