#include "test.h"
#include "../core/channel.h"
#include "../core/chatcommand.h"
#include "../core/logger.h"

int call_counter = 0;

class MyTestModule : public IModule {
public:

	void setupCommands(ChatCommand &cmd)
	{
		cmd.setMain((ChatCommandAction)&MyTestModule::mainCommand);
		cmd.add("!help", (ChatCommandAction)&MyTestModule::normalCommand);
	}

	bool normalCommand(Channel *c, UserInstance *ui, std::string msg)
	{
		call_counter++;
		return true;
	}

	bool mainCommand(Channel *c, UserInstance *ui, std::string msg)
	{
		call_counter--;
		return false;
	}
};

void test_Chatcommand_simple()
{
	MyTestModule mymod;
	std::string msg("!foo bar baz");

	{
		ChatCommand cmd(&mymod);
		mymod.setupCommands(cmd);
		TEST_CHECK(cmd.getList().size() > 0);

		TEST_CHECK(cmd.run(nullptr, nullptr, msg) == false);
		msg = "!help";
		TEST_CHECK(cmd.run(nullptr, nullptr, msg) == true);
		TEST_CHECK(call_counter == 1);
		cmd.remove(&mymod);
		TEST_CHECK(cmd.getList().size() == 0);
	}
	{
		ChatCommand cmd(&mymod);
		mymod.setupCommands(cmd);
		TEST_CHECK(cmd.run(nullptr, nullptr, msg) == false);
		TEST_CHECK(call_counter == 0);
	}
}

void test_Chatcommand(Unittest *ut)
{
	TEST_REGISTER(test_Chatcommand_simple)
}

