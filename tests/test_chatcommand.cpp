#include "test.h"
#include "../core/channel.h"
#include "../core/chatcommand.h"
#include "../core/logger.h"

int call_counter = 0;

class MyTestModule : public IModule {
public:
	void setupCommands(ChatCommand &cmd, bool assign_main)
	{
		if (assign_main)
			cmd.setMain((ChatCommandAction)&MyTestModule::mainCommand);
		cmd.add("!help", (ChatCommandAction)&MyTestModule::normalCommand);
	}

	bool normalCommand(Channel *c, UserInstance *ui, std::string msg)
	{
		call_counter += 10;
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
	call_counter = 0;

	{
		ChatCommand cmd(&mymod);
		mymod.setupCommands(cmd, true);
		TEST_CHECK(cmd.getList().size() > 0);

		// Executes main command -> -1
		TEST_CHECK(cmd.run(nullptr, nullptr, "!foo bar baz") == true);
		TEST_CHECK(call_counter == -1);

		// Execute valid "!help" command -> 9
		TEST_CHECK(cmd.run(nullptr, nullptr, "!help") == true);
		TEST_CHECK(call_counter == 9);
		cmd.remove(&mymod);
		TEST_CHECK(cmd.getList().size() == 0);
	}
	{
		// Without main command
		ChatCommand cmd(&mymod);
		mymod.setupCommands(cmd, false);
		TEST_CHECK(cmd.run(nullptr, nullptr, "!help") == true);
		TEST_CHECK(call_counter == 19);
		// Unhandled case
		TEST_CHECK(cmd.run(nullptr, nullptr, "!invalid") == false);
	}
}

void test_Chatcommand(Unittest *ut)
{
	TEST_REGISTER(test_Chatcommand_simple)
}

