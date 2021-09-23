#include "../core/channel.h"
#include "../core/chatcommand.h"
#include "../core/module.h"
#include "../core/utils.h"
#include <algorithm> // std::sort
#include <iostream>
#include <sstream>

// Per-channel-member
struct LPlayer : public IContainer {
	static std::string format(const std::vector<const char *> &cards)
	{
		std::ostringstream ss;
		ss << "\x0F"; // Normal text + Bold start
		for (const char *c : cards) {
			std::string str("\x02[" + std::string(c) + "] ");

			if (*c == 'Q') // Red queens
				ss << colorize_string(str, IC_RED);
			else // All other cards
				ss << str;
		}

		ss << "\x0F"; // Normal text
		return ss.str();
	}

	static void shuffle(std::vector<const char *> &cards)
	{
		// TODO, stub
	}

	std::vector<const char *> cards;

	static const char *FACES[];
};

const char *LPlayer::FACES[] = {
	"6", "7", "8", "9", "10", "J", "Q", "K"
};

// Per-channel
class LGame : public IContainer {
public:
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

	size_t getPlayerCount() const
	{ return m_players.size(); }

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
				p_it = m_players.end();
			c_it++;
		}

		has_started = true;
		current = *m_players.begin();
		return true;
	}

	bool has_started = false;
	UserInstance *current = nullptr;
	std::vector<const char *> top_cards;

private:
	static const size_t MIN_PLAYERS = 3;

	std::set<UserInstance *> m_players;
	Channel *m_channel;
	ChatCommand *m_commands;
};

class nbm_liar_game : public IModule {
public:
	nbm_liar_game()
	{
	}
};


extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_liar_game();
	}
}
