#include "../core/channel.h"
#include "../core/chatcommand.h"
#include "../core/module.h"
#include "../core/settings.h"
#include "../core/utils.h"
#include <algorithm> // std::sort
#include <iostream>
#include <random>
#include <sstream>

struct SCard {
	const char *face;
	const char *short_desc;
	const char *long_desc;
};

struct SPlayer : public IContainer {
	std::string dump() const { return "SPlayer"; }

	std::vector<const SCard *> cards;

	// Minimal count of cards in the player's hand if the stack is non-empty
	static const size_t MIN_IN_HAND = 3;
	static const SCard CARD_TYPES[];
};

const SCard SPlayer::CARD_TYPES[] = {
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

	bool start()
	{
		if (m_players.size() < SGame::MIN_PLAYERS || has_started)
			return false;

		// List of all cards in the game
		draw_stack.reserve(sizeof(SPlayer::CARD_TYPES) / sizeof(SCard) * SGame::NUM_PER_FACE);

		for (const auto &type : SPlayer::CARD_TYPES) {
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



