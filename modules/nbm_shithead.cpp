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
		for (auto c : cards) {
			ss << "[" << c->face;
			if (c->short_desc)
				ss << " " << c->short_desc;
			ss << "]";
		}
		return ss.str();
	}

	const char *face;
	const char *short_desc;
	const char *long_desc;

	static const SCard TYPES[];
};

const SCard SCard::TYPES[] = {
	{"2",  "(0)", "Reset to 0. Any card may be played."},
	{"3",  NULL,  "Initial game starting card."},
	{"4",  "(*)", "Wildcard: Preserves the previous number"},
	{"5",  NULL,  NULL},
	{"6",  NULL,  NULL},
	{"7",  "(v)", "The next played card must be lower than 7."},
	{"8",  NULL,  NULL},
	{"9",  NULL,  NULL},
	{"10", "(X)", "Wildcard: Terminates the stack."},
	{"J",  NULL,  NULL},
	{"Q",  NULL,  NULL},
	{"K",  NULL,  NULL},
	{"A",  NULL,  NULL},
};

struct SPlayer : public IContainer {
	std::string dump() const { return "SPlayer"; }

	std::vector<const SCard *> cards;

	// Minimal count of cards in the player's hand if the stack is non-empty
	static const size_t MIN_IN_HAND = 3;
};

class SGame : public IContainer {
public:
	std::string dump() const { return "SGame"; }

	SGame(Channel *c, ChatCommand *cmd) :
		m_channel(c), m_commands(cmd) {}

	~SGame()
	{
		for (UserInstance *ui : m_players) {
			m_commands->resetScope(m_channel, ui);
			ui->remove(this);
		}
	}

	inline SPlayer *getPlayer(UserInstance *ui)
	{
		return (SPlayer *)ui->get(this);
	}

	bool addPlayer(UserInstance *ui)
	{
		if (has_started || getPlayer(ui))
			return false;

		m_commands->setScope(m_channel, ui);

		SPlayer *p = new SPlayer();
		ui->set(this, p);
		m_players.insert(ui);
		return true;
	}

	void removePlayer(UserInstance *ui)
	{
		SPlayer *p = getPlayer(ui);
		if (!p)
			return;

		m_commands->resetScope(m_channel, ui);
		ui->remove(this);
		m_players.erase(ui);

		if (ui == current)
			turnNext();
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

	size_t getPlayerCount() const
	{ return m_players.size(); }

	bool start()
	{
		if (m_players.size() < SGame::MIN_PLAYERS || has_started)
			return false;

		// List of all cards in the game
		draw_stack.reserve(sizeof(SCard::TYPES) / sizeof(SCard) * SGame::NUM_PER_FACE);

		for (const SCard &type : SCard::TYPES) {
			for (int i = 0; i < SGame::NUM_PER_FACE; ++i)
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

		// Mark game as started
		has_started = true;
		current = *m_players.begin();
		return true;
	}

	static const size_t MIN_PLAYERS = 3;
	static const size_t NUM_PER_FACE = 4;

	bool has_started = false;
	UserInstance *current = nullptr;
	std::vector<const SCard *> draw_stack;
	std::vector<const SCard *> play_stack;

private:
	std::set<UserInstance *> m_players;
	Channel *m_channel;
	ChatCommand *m_commands;
};


class nbm_shithead : public IModule {
public:
	void onClientReady()
	{
		ChatCommand &uno = getModuleMgr()->getChatCommand()->add("$shithead", this);
		uno.setMain((ChatCommandAction)&nbm_shithead::cmd_help);
		uno.add("join",  (ChatCommandAction)&nbm_shithead::cmd_join);
		uno.add("leave", (ChatCommandAction)&nbm_shithead::cmd_leave);
		uno.add("p",     (ChatCommandAction)&nbm_shithead::cmd_play);
		uno.add("d",     (ChatCommandAction)&nbm_shithead::cmd_draw);
		m_commands = &uno;
	}

	inline SGame *getGame(Channel *c)
	{
		return (SGame *)c->getContainers()->get(this);
	}

	void onChannelLeave(Channel *c)
	{
		c->getContainers()->remove(this);
	}

	void onUserLeave(Channel *c, UserInstance *ui)
	{
		if (SGame *g = getGame(c)) {
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
		SGame *g = getGame(c);
		if (!g)
			return false;

		if (g->getPlayerCount() >= SGame::MIN_PLAYERS)
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
		ss << "Top card: [" << g->play_stack.back() << "]";
		ss << " - Size: "<< g->play_stack.size();
		if (!g->draw_stack.empty())
			ss << " (Draw stack: "<< g->play_stack.size() << ")";

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
		if (g->getPlayerCount() < SGame::MIN_PLAYERS)
			ss << "Miniaml player count: " << SGame::MIN_PLAYERS;
		else
			ss << "You may start it now.";
		c->say(ss.str());
	}

	// TODO: Move those functions into a parent/helper class
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
		SGame *g = getGame(c);
		SPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!p || !g->has_started) {
			c->notice(ui, "huh?? You don't have any cards.");
			return;
		}

		if (g->current != ui) {
			c->notice(ui, "It is not your turn (current: " + g->current->nickname + ").");
			return;
		}

		const SCard *played_card = nullptr;
		long played_count = 1;
		{
			std::string card_s(get_next_part(msg));
			std::string count_s(get_next_part(msg));

			for (char &c : card_s)
				c = toupper(c);

			for (auto &type : SCard::TYPES) {
				if (type.face == card_s) {
					played_card = &type;
					break;
				}
			}

			SettingType::parseLong(count_s, &played_count);
		}

		if (!played_card) {
			c->notice(ui, "Invalid card type. Check the spelling.");
			return;
		}

		// TODO: CHeck whether this card may be played.

		size_t have_num = std::count_if(p->cards.begin(), p->cards.end(), [=] (const SCard *a) -> bool {
			return a == played_card;
		});

		if (have_num < played_count) {
			c->notice(ui, "You do not have enough cards of this type.");
			return;
		}

		std::remove_if(p->cards.begin(), p->cards.end(), [&] (const SCard *a) -> bool {
			if (a == played_card && played_count > 0) {
				played_count--;
				return true;
			}
			return false;
		});


		// Refill cards in hand
		bool new_cards = false;
		while (!g->draw_stack.empty() && p->cards.size() < SPlayer::MIN_IN_HAND) {
			p->cards.push_back(g->draw_stack.back());
			g->draw_stack.pop_back();
			new_cards = true;
		}
		if (new_cards)
			std::sort(p->cards.begin(), p->cards.end());

		tellGameStatus(c);
	}

	CHATCMD_FUNC(cmd_draw)
	{
		SGame *g = getGame(c);
		SPlayer *p = g ? g->getPlayer(ui) : nullptr;
		if (!p || !g->has_started) {
			c->notice(ui, "You are not part of an ongoing game.");
			return;
		}

		if (g->current != ui) {
			c->notice(ui, "It is not your turn (current: " + g->current->nickname + ").");
			return;
		}

		if (!g->draw_stack.empty()) {
			// Draw from stack
		} else {
			// Draw the played stack
			p->cards.insert(p->cards.end(), g->play_stack.begin(), g->play_stack.end());
			std::sort(p->cards.begin(), p->cards.end());
		}
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



