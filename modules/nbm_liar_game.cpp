#include "../core/framework_game.h"

#include "../core/channel.h"
#include "../core/chatcommand.h"
#include "../core/module.h"
#include "../core/settings.h"
#include "../core/utils.h"
#include <algorithm> // std::sort
#include <iostream>
#include <memory> // std::unique_ptr
#include <random>
#include <sstream>

// Per-channel-member
struct LPlayer : public IContainer {
	std::string dump() const { return "LPlayer"; }

	static void format(IFormatter *fmt, const std::vector<const char *> &cards,
		bool hand_info = false)
	{
		if (hand_info)
			*fmt << "Your cards: ";

		fmt->begin(IC_BLACK);
		fmt->end(FT_COLOR); // Clear previous formatting
		for (size_t i = 0; i < cards.size(); ++i) {
			if (hand_info)
				*fmt << (i + 1);

			std::string inner("[" + std::string(cards[i]) + "]");
			fmt->begin(FT_BOLD);
			if (*(cards[i]) == 'Q') {
				// Red queens
				fmt->begin(IC_RED);
				*fmt << inner;
				fmt->end(FT_COLOR);
			} else // All other cards
				*fmt << inner;
			fmt->end(FT_BOLD);

			if (i + 1 < cards.size())
				*fmt << ' ';
		}
	}

	static std::string format(IUserOwner *iuo, const std::vector<const char *> &cards,
		bool hand_info = false)
	{
		std::unique_ptr<IFormatter> fmt(iuo->createFormatter());
		format(fmt.get(), cards, hand_info);
		return fmt->str();
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
class LGame : public GameF_internal<LPlayer> {
public:
	std::string dump() const { return "LGame"; }

	LGame(Channel *c, ChatCommand *cmd) :
		GameF_internal(c, cmd, 3) {}

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

	const std::set<UserInstance *> &getAllPlayers() const
	{ return m_players; }

	bool start()
	{
		if (!GameF_internal::start())
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

		return true;
	}

	std::vector<const char *> stack;
	const char *main_face = nullptr;
	size_t stack_last = 0;
};

class nbm_liar_game : public GameF_nbm<LGame, LPlayer> {
public:
	void onClientReady()
	{
		ChatCommand &lcmd = getModuleMgr()->getChatCommand()->add("$lgame", this);
		lcmd.setMain((ChatCommandAction)&nbm_liar_game::cmd_help);
		lcmd.add("join",  (ChatCommandAction)&nbm_liar_game::cmd_join, this);
		lcmd.add("leave", (ChatCommandAction)&nbm_liar_game::cmd_leave, this);
		lcmd.add("start", (ChatCommandAction)&nbm_liar_game::cmd_start, this);
		lcmd.add("add",   (ChatCommandAction)&nbm_liar_game::cmd_add, this);
		lcmd.add("p",     (ChatCommandAction)&nbm_liar_game::cmd_add, this);
		lcmd.add("check", (ChatCommandAction)&nbm_liar_game::cmd_check, this);
		lcmd.add("cards", (ChatCommandAction)&nbm_liar_game::cmd_cards, this);
		m_commands = &lcmd;
	}

	void onUserLeave(Channel *c, UserInstance *ui)
	{
		GameF_onUserLeave(c, ui);
	}

	void onChannelLeave(Channel *c)
	{
		c->getContainers()->remove(this);
	}

	void tellGameStatus(Channel *c, UserInstance *to_user = nullptr)
	{
		LGame *g = getGame(c);
		if (!g->has_started || g->stack.empty())
			return;

		if (!g->current)
			return; // Should not happen

		// No specific player. Take current one
		if (to_user == nullptr)
			to_user = g->current;

		// Normal ongoing game
		std::unique_ptr<IFormatter> fmt(c->createFormatter());
		*fmt << "[LGame] Main card: ";
		LPlayer::format(fmt.get(), { g->main_face });
		*fmt << ", Stack height: " << g->stack.size();
		*fmt << ". Current player: " << g->current->nickname;
		c->say(fmt->str());

		c->notice(to_user, LPlayer::format(c, g->getPlayer(to_user)->cards, true));
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
			if (!played_card) {
				c->say(ui->nickname + " has no cards left. Congratulations, you're a winner!");
				g->removePlayer(ui);
				return true;
			}

			std::unique_ptr<IFormatter> fmt(c->createFormatter());
			fmt->mention(ui);
			*fmt << " played ";
			fmt->begin(IC_ORANGE); *fmt << "their last card"; fmt->end(FT_COLOR);
			*fmt << "!";
			c->say(fmt->str());

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
				std::unique_ptr<IFormatter> fmt(c->createFormatter());
				fmt->mention(ui);
				*fmt << " can discard four ";
				LPlayer::format(fmt.get(), discarded);
				*fmt << " cards. Left cards: " << p->cards.size();

				c->say(fmt->str());
	
				// Re-check for win
				return updateCardsWin(c, ui, false);
			}
			return false;
		}

		if (amount <= 3 && ui == g->current) {
			std::unique_ptr<IFormatter> fmt(c->createFormatter());
			fmt->mention(ui);
			*fmt << " has ";
			fmt->begin(IC_ORANGE); *fmt << "only " << amount << " cards"; fmt->end(FT_COLOR);
			*fmt << " left!";
			c->say(fmt->str());
		}
		return false;
	}

	bool processGameUpdate(Channel *c)
	{
		LGame *g = getGame(c);
		if (!g)
			return false;
		if (g->getAllPlayers().size() >= g->MIN_PLAYERS)
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
		if (count >= g->MIN_PLAYERS)
			ss << " If you want to start the game, use \"$start\"";
		else if (count == 1)
			ss << " At least " << g->MIN_PLAYERS << " players are required to start the game.";

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
				+ std::to_string(g->MIN_PLAYERS) + " players waiting.");
			return;
		}

		ui = g->current;
		c->say("Game started! Player " + ui->nickname +
			" may play the first card using \"$p <'main card'> <card nr.> [<card nr.> [<card nr.>]]\"" +
			" (Card nr. from your hand)");

		for (UserInstance *ui : g->getAllPlayers())
			updateCardsWin(c, ui, false);
	
		c->notice(ui, LPlayer::format(c, g->getPlayer(ui)->cards, true));
	}

	CHATCMD_FUNC(cmd_add)
	{
		if (!checkPlayerTurn(c, ui))
			return;
		LGame *g = getGame(c);
		LPlayer *p = g->getPlayer(ui);

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
				std::unique_ptr<IFormatter> fmt(c->createFormatter());
				*fmt << "Wrong card type! Please pretend to place a card of type ";
				LPlayer::format(fmt.get(), { g->main_face });
				c->notice(ui, fmt->str());
				return;
			}
		}

		// Face is specified. Add cards.
		std::set<size_t> indices;
		while (!msg.empty()) {
			std::string index(get_next_part(msg));
			int64_t out = -1;
			if (!SettingType::parseS64(index, &out) ||
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

		// Previously played cards were not checked -> player wins.
		updateCardsWin(c, g->getPrevPlayer(), false);
		// Current player may not win yet
		if (!updateCardsWin(c, ui, true))
			g->turnNext();

		if (!processGameUpdate(c))
			return; // Game ended

		tellGameStatus(c);
	}

	CHATCMD_FUNC(cmd_check)
	{
		if (!checkPlayerTurn(c, ui))
			return;
		LGame *g = getGame(c);
		LPlayer *p = g->getPlayer(ui);

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

		std::unique_ptr<IFormatter> fmt(c->createFormatter());
		if (contains_invalid) {
			*fmt << "One or more top cards were not a [" << g->main_face << "].";
		} else {
			*fmt << "The top cards were correct!";

			g->turnNext();
		}

		*fmt << " (";
		LPlayer::format(fmt.get(), top_cards);
		*fmt << ") ";

		// Previous player draws the cards
		UserInstance *ui_prev = g->getPrevPlayer();
		LPlayer *p_prev = g->getPlayer(ui_prev);
		p_prev->cards.insert(p_prev->cards.end(), g->stack.begin(), g->stack.end());

		g->stack.clear();
		g->stack_last = 0;
		g->main_face = nullptr;

		*fmt << "Complete stack goes to " << ui_prev->nickname << ". ";

		*fmt << g->current->nickname + " may start with an empty stack.";
		c->say(fmt->str());

		// Show newest game status
		size_t num_players = g->getAllPlayers().size();
		ui = g->current;
		p = g->getPlayer(ui);

		if (updateCardsWin(c, ui_prev, false)) {
			// User played their last card and it was correct
		} else {
			c->notice(ui_prev, LPlayer::format(c, p_prev->cards, true));
		}
		if (updateCardsWin(c, ui, false)) {
			// Obscure win: discard all cards in hand
		} else {
			c->notice(ui, LPlayer::format(c, p->cards, true));
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
		c->notice(ui, LPlayer::format(c, p->cards, true));
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
