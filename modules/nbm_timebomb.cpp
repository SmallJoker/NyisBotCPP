#include "../core/channel.h"
#include "../core/chatcommand.h"
#include "../core/module.h"
#include "../core/utils.h"

struct MyTimebomb : public IContainer {
	std::string dump() const { return "MyTimebomb"; }

	void cooldown()
	{
		timer = (get_random() % 20) + 50;
		victim = nullptr;
	}
	void blast(Channel *c)
	{
		if ((get_random() % 100) > 10)
			c->say("*** BOOOM ***  Rest in peace, " + victim->nickname);
		else
			c->say("The bomb appears to be defective.. bad quality.");

		cooldown();
	}

	float timer = -1;
	UserInstance *victim = nullptr;
	std::vector<const char *> colors;
	const char *good_wire;
};

class nbm_timebomb : public IModule {
public:
	void onClientReady()
	{
		ChatCommand &cmd = *getModuleMgr()->getChatCommand();
		cmd.add("$timebomb", (ChatCommandAction)&nbm_timebomb::cmd_timebomb, this);
		cmd.add("$cutwire",  (ChatCommandAction)&nbm_timebomb::cmd_cutwire, this);
	}

	void onUserLeave(Channel *c, UserInstance *ui)
	{
		MyTimebomb *tb = (MyTimebomb *)ui->get(this);
		if (tb && tb->victim == ui)
			tb->victim = nullptr;
	}

	void onStep(float time)
	{
		for (Channel *c : getNetwork()->getAllChannels()) {
			MyTimebomb *tb = (MyTimebomb *)c->getContainers()->get(this);
			if (!tb)
				continue;

			if (tb->timer < 0)
				continue;

			tb->timer -= time;
			if (tb->victim && tb->timer < 0)
				tb->blast(c);
		}
	}

	bool onUserSay(Channel *c, UserInstance *ui, std::string &msg)
	{
		std::string cmd(get_next_part(msg));
		if (cmd != "$cutewire" && cmd != "$cutwrite" && cmd != "$cutewrite")
			return false;

		MyTimebomb *tb = (MyTimebomb *)c->getContainers()->get(this);
		if (!tb->victim)
			return false;

		c->say(ui->nickname + ": NOOOOOO what are you doing?! Are you insane?");
		tb->victim = ui;
		tb->timer *= 0.5f;
		return true;
	}

	CHATCMD_FUNC(cmd_timebomb)
	{
		MyTimebomb *tb = (MyTimebomb *)c->getContainers()->get(this);
		if (!tb) {
			tb = new MyTimebomb();
			c->getContainers()->set(this, tb);
		}

		if (tb->victim) {
			c->say(ui->nickname + ": Are you insane? " + tb->victim->nickname +
				" is currently busy with their timebomb!");
			return;
		}
		if (tb->timer > 0) {
			c->say(ui->nickname + ": Preparing materials for a new bomb. Please wait.");
			return;
		}
		std::string nick(get_next_part(msg));
		UserInstance *victim = c->getUser(nick);
		if (!victim) {
			c->say("Oh what a shame. Cannot find this user. I'll pick you instead.");
			victim = ui;
		}

		tb->timer = (get_random() % 30) + 40;
		tb->colors.clear();

		int howmany = (get_random() % 2) + 2;
		std::string which;
		while (howmany --> 0) {
			tb->colors.push_back(s_colors[get_random() % COLORS]);
			if (!which.empty())
				which.append(", ");
			which.append(tb->colors.back());
		}
		tb->good_wire = tb->colors[get_random() % tb->colors.size()];

		tb->victim = victim;
		c->say(victim->nickname + ": OUCH! Someone planted a bomb! "
			+ std::to_string((int)tb->timer) + " seconds until explosion. "
			"Try $cutwire <color> from one of those: " + which);
	}

	CHATCMD_FUNC(cmd_cutwire)
	{
		MyTimebomb *tb = (MyTimebomb *)c->getContainers()->get(this);
		if (!tb->victim)
			return;

		std::string selected(get_next_part(msg));
		for (char &c : selected)
			c = tolower(c);

		if (selected == tb->good_wire) {
			tb->cooldown();
			c->say(ui->nickname + ": Uh oh! You successfully disarmed the bomb.");
			return;
		}

		bool found = false;
		for (size_t i = 0; i < COLORS; ++i) {
			if (s_colors[i] == selected) {
				found = true;
				break;
			}
		}
		if (found) {
			tb->victim = ui;
			tb->blast(c);
		} else {
			c->say(ui->nickname + " is confused. There's no such wire color!");
		}
	}

private:
	static const size_t COLORS = 11;
	static const char *s_colors[];
};

const char *nbm_timebomb::s_colors[nbm_timebomb::COLORS] = {
	"black",
	"blue",
	"brown",
	"green",
	"orange",
	"purple",
	"red",
	"turquoise",
	"violet",
	"white",
	"yellow"
};

extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_timebomb();
	}
}
