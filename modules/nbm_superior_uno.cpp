#include "../core/channel.h"
#include "../core/chatcommand.h"
#include "../core/module.h"
#include "../core/settings.h"
#include "../core/utils.h"
#include <cstring>
#include <parallel/sort.h>

struct Card {
	enum CardColor {
		CC_NONE,
		CC_RED,
		CC_GREEN,
		CC_BLUE,
		CC_YELLOW,
		CC_COLORS_MAX
	} color = CC_COLORS_MAX;
	const char *face = nullptr;

	static const size_t TYPES_MAX = 15;
	static const char *TYPES[];
	static const int COLORS[];  // IRC colors
};

const char *Card::TYPES[Card::TYPES_MAX] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "D2", "R", "S", "W", "WD4"
};

const int Card::COLORS[Card::CC_COLORS_MAX] = { 1, 4, 3, 2, 8 };

class UnoPlayer : public IContainer, public SettingType {
public:
	bool deSerialize(cstr_t &str)
	{
		auto parts = strsplit(str);
		if (parts.size() < 4)
			return false;

		return parseInt(parts[0], &m_wins)
			&& parseInt(parts[1], &m_losses)
			&& parseInt(parts[2], &m_elo)
			&& parseInt(parts[3], &m_streak);
	}

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
			auto color = static_cast<Card::CardColor>(get_random() % (int)Card::CC_COLORS_MAX);
			const char *face = Card::TYPES[get_random() % Card::TYPES_MAX];

			if (strchr(face, 'W')) {
				color = Card::CC_NONE; // must be black

				if (max_special == 0)
					continue; // Retry
				max_special--;
			} else if (color == Card::CC_NONE) {
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

	void win(std::set<UserInstance *> &players, int initial_player_count)
	{
		// stub
	}

	std::string formatElo(bool details)
	{
		return "stub";
	}

	void sortCards()
	{
		std::sort(cards.begin(), cards.end(), [&] (const Card &a, const Card &b) -> int {
			if (a.color == b.color)
				return strcmp(a.face, b.face);
			return (a.color > b.color) - (a.color < b.color);
		});
	}

	std::vector<Card> cards;
private:
	int m_wins = 0,
		m_losses = 0,
		m_streak = 0,
		m_elo = 1000;
};

class UnoGame : public IContainer {
public:
	UnoGame(uint8_t modes, Channel *c, Settings *s) :
		m_channel(c), m_settings(s), m_modes(modes) {}

	~UnoGame()
	{
		for (UserInstance *ui : m_players)
			ui->remove(this);
	}

	enum UnoMode : uint8_t
	{
		UM_STACK_D2  = 0x01, // Stack "draw +2" cards
		UM_STACK_WD4 = 0x02, // Stack "wild draw +4" cards
		UM_UPGRADE   = 0x04, // Place "wild draw +4" onto "draw +2"
		UM_MULTIPLE  = 0x08, // Place same cards (TODO)
		UM_LIGRETTO  = 0x40, // Smash in cards whenever you have a matching one
		UM_RANKED    = 0x80, // Ranked game, requires auth to join
	};

	bool checkMode(UnoMode mode)
	{ return (m_modes & (uint8_t)mode) != 0; }

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

	void addPlayer(UserInstance *ui)
	{
		ui->set(this, new UnoPlayer());
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
				player->win(m_players, m_initial_player_count);

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

private:
	Channel *m_channel;
	Settings *m_settings;
	std::set<UserInstance *> m_players;

	uint8_t m_modes;
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
	}

	void onClientReady()
	{
		if (m_settings)
			return;

		m_settings = getModuleMgr()->getSettings(this);
		m_settings->syncFileContents();
	}

	CHATCMD_FUNC(cmd_join)
	{
		
	}

	CHATCMD_FUNC(cmd_leave)
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
