#pragma once

#include "../core/channel.h"
#include "../core/chatcommand.h"
#include "../core/module.h"

/*
	This is a base class for turn-based games.
*/

template <typename PlayerT>
class GameF_internal : public IContainer {
public:
	DISABLE_COPY(GameF_internal);

	~GameF_internal()
	{
		for (UserInstance *ui : m_players) {
			m_commands->resetScope(m_channel, ui);
			ui->remove(this);
		}
	}

	PlayerT *getPlayer(UserInstance *ui)
	{
		return (PlayerT *)ui->get(this);
	}

	bool addPlayer(UserInstance *ui)
	{
		if (has_started || getPlayer(ui))
			return false;

		m_commands->setScope(m_channel, ui);

		PlayerT *p = new PlayerT();
		ui->set(this, p);
		m_players.insert(ui);
		return true;
	}

	bool removePlayer(UserInstance *ui)
	{
		PlayerT *player = getPlayer(ui);
		if (!player)
			return false;

		if (ui == current)
			turnNext();

		m_commands->resetScope(m_channel, ui);

		ui->remove(this);
		m_players.erase(ui);
		return true;
	}

	size_t getPlayerCount() const
	{ return m_players.size(); }

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

	bool start()
	{
		if (m_players.size() < MIN_PLAYERS || has_started)
			return false;

		has_started = true;
		auto it = m_players.begin();
		std::advance(it, get_random() % m_players.size());
		current = *it;

		return true;
	}

	const size_t MIN_PLAYERS;
	bool has_started = false;
	UserInstance *current = nullptr;
	bool dir_forwards = true;

protected:
	GameF_internal(Channel *c, ChatCommand *cmd, size_t min_players) :
		MIN_PLAYERS(min_players),
		m_channel(c),
		m_commands(cmd) {}

	std::set<UserInstance *> m_players;
	Channel *m_channel;
	ChatCommand *m_commands;
	
};

// IMPORTANT! Only use linear inheritance. Otherwise "this" may change!
template <typename GameT, typename PlayerT>
class GameF_nbm : public IModule {
public:
	virtual void tellGameStatus(Channel *c, UserInstance *to_user = nullptr)
	{
		c->say("tellGameStatus: INVALID USAGE");
	}

	virtual bool processGameUpdate(Channel *c)
	{
		c->say("processGameUpdate: INVALID USAGE");
		return false;
	}

	GameT *getGame(Channel *c)
	{
		return (GameT *)c->getContainers()->get(this);
	}

	// ====== Callbacks for use in the inherited class
	// ===============================================

	void GameF_onUserLeave(Channel *c, UserInstance *ui)
	{
		if (GameT *g = getGame(c)) {
			UserInstance *old_current = g->current;
			g->removePlayer(ui);

			if (processGameUpdate(c)) {
				if (old_current != g->current)
					tellGameStatus(c);
			}
		}
	}

	bool checkPlayerTurn(Channel *c, UserInstance *ui)
	{
		GameT *g = getGame(c);
		PlayerT *p = g ? g->getPlayer(ui) : nullptr;
		if (!p || !g->has_started) {
			c->notice(ui, "You are not part of an ongoing game.");
			return false;
		}

		if (g->current != ui) {
			c->notice(ui, "It is not your turn (current: " + g->current->nickname + ").");
			return false;
		}
		return true;
	}

protected:
	GameF_nbm() {}
};
