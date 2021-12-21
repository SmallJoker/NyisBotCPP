#include "../core/framework_game.h"

#include "../core/channel.h"
#include "../core/chatcommand.h"
#include "../core/module.h"
#include "../core/settings.h"
#include "../core/utils.h"
#include <algorithm> // std::sort
#include <iostream>
#include <random>
#include <sstream>
#include <cstring> // strcmp

struct SCard {
	bool operator<(const SCard &b)
	{
		return strcmp(face, b.face) < 0;
	}

	static std::string format(const std::vector<const SCard *> &cards)
	{
		std::ostringstream ss;
		ss << " ";
		for (auto c : cards) {
			ss << "[" << c->face;
			if (c->short_desc)
				ss << " " << c->short_desc;
			ss << "] ";
		}
		return ss.str();
	}

	enum CardAction {
		CA_NONE,
		CA_2_RESET,
		CA_4_WILD,
		CA_7_LOWER,
		CA_10_DONE,
	} action;

	const char *face;
	const char *short_desc;
	const char *long_desc;

	static const SCard TYPES[];
};

// In sync with CardType
const SCard SCard::TYPES[] = {
	{CA_2_RESET, "2",  "⁰", "Reset to 0. Any card may be played."},
	{CA_NONE,    "3",  NULL,  NULL},
	{CA_4_WILD,  "4",  "*", "Wildcard: Preserves the previous number"},
	{CA_NONE,    "5",  NULL,  NULL},
	{CA_NONE,    "6",  NULL,  NULL},
	{CA_7_LOWER, "7",  "v", "The next played card must be lower than 7."},
	{CA_NONE,    "8",  NULL,  NULL},
	{CA_NONE,    "9",  NULL,  NULL},
	{CA_10_DONE, "10", "x", "Wildcard: Terminates the stack."},
	{CA_NONE,    "J",  NULL,  NULL},
	{CA_NONE,    "Q",  NULL,  NULL},
	{CA_NONE,    "K",  NULL,  NULL},
	{CA_NONE,    "A",  NULL,  NULL},
};

struct SPlayer : public IContainer {
	std::string dump() const { return "SPlayer"; }

	std::vector<const SCard *> cards;

	// Minimal count of cards in the player's hand if the stack is non-empty
	static const size_t MIN_IN_HAND = 5;
};

class SGame : public IContainer, public GameF_internal<SPlayer> {
public:
	std::string dump() const { return "SGame"; }

	SGame(Channel *c, ChatCommand *cmd) :
		GameF_internal<SPlayer>(c, cmd, 2) {}

	~SGame()
	{
		// Cleanup done by framework
	}

	bool removePlayer(UserInstance *ui)
	{
		SPlayer *p = getPlayer(ui);
		if (!p)
			return false;

		if (has_started && p->cards.size() == 0)
			m_channel->say(ui->nickname + " finished the game. gg!");

		return GameF_internal::removePlayer(ui);
	}

	bool start()
	{
		if (!GameF_internal::start())
			return false;

		// List of all cards in the game
		draw_stack.reserve(sizeof(SCard::TYPES) / sizeof(SCard) * SGame::NUM_PER_FACE);

		for (const SCard &type : SCard::TYPES) {
			for (size_t i = 0; i < SGame::NUM_PER_FACE; ++i)
				draw_stack.emplace_back(&type);
		}

		// Shuffle cards
		std::random_shuffle(draw_stack.begin(), draw_stack.end(), [] (int i) -> int {
			return std::rand() % i;
		});

		// Distribute cards to each player
		auto c_it = draw_stack.end();
		for (auto p : m_players) {
			auto &cards = getPlayer(p)->cards;
			cards.reserve(SPlayer::MIN_IN_HAND);

			for (size_t i = 0; i < SPlayer::MIN_IN_HAND; ++i) {
				c_it--;
				cards.emplace_back(*c_it);
			}
			std::sort(cards.begin(), cards.end());
		}
		draw_stack.erase(c_it, draw_stack.end());

		return true;
	}

	static const size_t NUM_PER_FACE = 5;

	std::vector<const SCard *> draw_stack;
	std::vector<const SCard *> play_stack;
};


class nbm_shithead : public GameF_nbm<SGame, SPlayer> {
public:
	void onClientReady()
	{
		ChatCommand &uno = getModuleMgr()->getChatCommand()->add("$shithead", this);
		uno.setMain((ChatCommandAction)&nbm_shithead::cmd_help);
		uno.add("join",  (ChatCommandAction)&nbm_shithead::cmd_join);
		uno.add("leave", (ChatCommandAction)&nbm_shithead::cmd_leave);
		uno.add("deal",  (ChatCommandAction)&nbm_shithead::cmd_deal);
		uno.add("top",   (ChatCommandAction)&nbm_shithead::cmd_top);
		uno.add("p",     (ChatCommandAction)&nbm_shithead::cmd_play);
		uno.add("d",     (ChatCommandAction)&nbm_shithead::cmd_draw);
		uno.add("t",     (ChatCommandAction)&nbm_shithead::cmd_take_stack);
		m_commands = &uno;
	}

	void onChannelLeave(Channel *c)
	{
		c->getContainers()->remove(this);
	}

	void onUserLeave(Channel *c, UserInstance *ui)
	{
		GameF_onUserLeave(c, ui);
	}

	// Returns whether the game is still active
	bool processGameUpdate(Channel *c)
	{
		SGame *g = getGame(c);
		if (!g)
			return false;

		if (g->getPlayerCount() >= g->MIN_PLAYERS)
			return true;

		if (g->has_started) {
			c->say("[Shithead] Game ended");
		} else if (g->getPlayerCount() > 0) {
			// Not started yet. Wait for players.
			return false;
		}

		c->getContainers()->remove(this);
		return false;
	}

	void tellGameStatus(Channel *c, UserInstance *to_user = nullptr)
	{
		SGame *g = getGame(c);
		if (!g)
			return;

		// No specific player. Take current one
		if (to_user == nullptr)
			to_user = g->current;

		SPlayer *p = g->getPlayer(g->current);

		std::ostringstream ss;
		ss << "[Shithead] " << g->current->nickname << " (" << p->cards.size() << " cards) - ";
		if (g->play_stack.empty()) {
			ss << "Empty stack (start with any low number)";
		} else {
			ss << "Top card:\x02" << SCard::format({ g->play_stack.back() }) << "\x0F"; // Bold, then normal.
			ss << "Size: "<< g->play_stack.size();
		}
		if (!g->draw_stack.empty())
			ss << " - (Draw stack: "<< g->draw_stack.size() << ")";

		c->say(ss.str());

		c->notice(to_user, "Your cards: " + SCard::format(g->getPlayer(to_user)->cards));
	}

	CHATCMD_FUNC(cmd_help)
	{
		std::string what(get_next_part(msg));
		if (what.empty()) {
			c->say("Available subcommands: " + m_commands->getList());
			return;
		}

		// help <card type>
		for (char &c : what)
			c = toupper(c);

		const SCard *which = nullptr;
		for (const SCard &type : SCard::TYPES) {
			if (type.face == what) {
				which = &type;
				break;
			}
		}
		if (!which) {
			c->say("No help for unknown card type \"" + what + "\"");
			return;
		}

		std::ostringstream ss;
		ss << "Description of card type [" << which->face << "]: "<< which->long_desc;
		c->reply(ui, ss.str());
	}

	CHATCMD_FUNC(cmd_join)
	{
		SGame *g = getGame(c);
		if (g && g->has_started) {
			c->reply(ui, "Please wait for " + g->current->nickname + " to finish their game.");
			return;
		}

		if (!g) {
			g = new SGame(c, m_commands);
			c->getContainers()->set(this, g);
		}

		if (!g->addPlayer(ui)) {
			c->notice(ui, "You already joined the game");
			return;
		}

		std::ostringstream ss;
		ss << "[Shithead] " << g->getPlayerCount();
		ss << " player(s) are waiting for a new Shithead game. ";
		if (g->getPlayerCount() < g->MIN_PLAYERS)
			ss << "Miniaml player count: " << g->MIN_PLAYERS;
		else
			ss << "You may start it now.";
		c->say(ss.str());
	}

	CHATCMD_FUNC(cmd_leave)
	{
		SGame *g = getGame(c);
		SPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!g || !p) {
			c->notice(ui, "There is nothing to leave...");
			return;
		}
		onUserLeave(c, ui);
	}

	CHATCMD_FUNC(cmd_deal)
	{
		SGame *g = getGame(c);
		SPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!p || g->has_started) {
			c->notice(ui, "There is no game to start.");
			return;
		}
		if (!g->start()) {
			c->say("Game start failed. Either there are not enough players, "
				"or the game is already ongoing.");
			return;
		}

		tellGameStatus(c);
	}

	CHATCMD_FUNC(cmd_top)
	{
		SGame *g = getGame(c);
		SPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!p || !g->has_started) {
			c->notice(ui, "You are not part of an ongoing game.");
			return;
		}
		tellGameStatus(c, ui);
	}

	CHATCMD_FUNC(cmd_play)
	{
		if (!checkPlayerTurn(c, ui))
			return;
		SGame *g = getGame(c);
		SPlayer *p = g->getPlayer(ui);

		const SCard *played_card = nullptr;
		long played_count = 1;
		// Get card type and count
		{
			std::string card_s(get_next_part(msg));
			std::string count_s(get_next_part(msg));

			for (char &c : card_s)
				c = toupper(c); // J Q K A

			for (auto &type : SCard::TYPES) {
				if (type.face == card_s) {
					played_card = &type;
					break;
				}
			}

			if (!count_s.empty())
				SettingType::parseLong(count_s, &played_count);
		}

		if (!played_card) {
			c->notice(ui, "Invalid card type. Check the spelling.");
			return;
		}

		// Start condition check
		const SCard *top = SCard::TYPES;

		// Walk backwards until a non-4 is found
		for (auto it = g->play_stack.rbegin(); it != g->play_stack.rend(); ++it) {
			if ((*it)->action != SCard::CA_4_WILD) {
				top = *it;
				break;
			}
		}

		// Check whether the card may be played
		bool ok = false;
		switch (played_card->action) {
			case SCard::CA_2_RESET:
			case SCard::CA_4_WILD:
			case SCard::CA_10_DONE:
				ok = true;
				break;
			default:
				// Pointer comparison
				if (top->action != SCard::CA_7_LOWER) {
					// Normal case
					ok = played_card >= top;
				} else {
					// Must be lower
					ok = played_card < top;
				}
				break;
		}

		if (!ok) {
			c->notice(ui, "This card cannot played. Generally play higher cards, "
				"or lower only if a [7] was played last.");
			return;
		}

		// Count cards in hand to play
		long have_num = std::count_if(p->cards.begin(), p->cards.end(), [=] (const SCard *a) -> bool {
			return a == played_card;
		});

		if (have_num < played_count) {
			c->notice(ui, "You do not have enough cards of this type.");
			return;
		}

		// Remove cards from hand, add to play stack
		for (auto it = p->cards.begin(); it != p->cards.end();){
			if (*it == played_card && played_count > 0) {
				g->play_stack.push_back(*it);
				played_count--;

				it = p->cards.erase(it);
				continue;
			}
			it++;
		}


		// Do card action
		if (played_card->action == SCard::CA_10_DONE)
			g->play_stack.clear();


		// Refill cards in hand
		{
			std::vector<const SCard *> drawn;
			for (size_t i = p->cards.size(); i < SPlayer::MIN_IN_HAND; ++i) {
				if (g->draw_stack.empty())
					break;
				drawn.push_back(g->draw_stack.back());
				g->draw_stack.pop_back();
			}

			if (!drawn.empty()) {
				c->notice(ui, "You drew the following cards: " + SCard::format(drawn));
				p->cards.insert(p->cards.end(), drawn.begin(), drawn.end());
				std::sort(p->cards.begin(), p->cards.end());
			}
		}

		g->turnNext();

		// Player won, except when it's again their turn (last card = skip)
		if (p->cards.empty() && g->current != ui)
			g->removePlayer(ui);

		if (!processGameUpdate(c))
			return; // Game ended

		tellGameStatus(c);
	}

	// Draw from the draw stack
	CHATCMD_FUNC(cmd_draw)
	{
		if (!checkPlayerTurn(c, ui))
			return;
		SGame *g = getGame(c);
		SPlayer *p = g->getPlayer(ui);

		if (g->draw_stack.empty()) {
			c->notice(ui, "There is no draw stack to draw from. "
				"Use the 't' subcommand to draw the entire played stack.");
			return;
		}

		// Draw from draw stack
		p->cards.push_back(g->draw_stack.back());
		g->draw_stack.pop_back();
		c->notice(ui, "You drew the following card: " + SCard::format({ p->cards.back() }));

		std::sort(p->cards.begin(), p->cards.end());

		g->turnNext();
		tellGameStatus(c);
	}

	// Draw the entire played stack
	CHATCMD_FUNC(cmd_take_stack)
	{
		if (!checkPlayerTurn(c, ui))
			return;
		SGame *g = getGame(c);
		SPlayer *p = g->getPlayer(ui);

		if (g->play_stack.empty()) {
			c->notice(ui, "Start a new stack using any card.");
			return;
		}

		// Draw the play stack
		p->cards.insert(p->cards.end(), g->play_stack.begin(), g->play_stack.end());
		std::sort(p->cards.begin(), p->cards.end());

		c->notice(ui, "You drew the following cards: " + SCard::format(g->play_stack));
		g->play_stack.clear();

		g->turnNext();
		tellGameStatus(c);
	}

private:
	ChatCommand *m_commands = nullptr;
};

extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_shithead();
	}
}



