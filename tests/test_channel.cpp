
#include "test.h"
#include "../core/channel.h"
#include "../core/client.h"
#include "../core/iimpl_basic.h"
#include "../core/logger.h"
#include "../core/settings.h"
#include "../core/utils.h"

static IClient *client = nullptr;

void test_Client_setup()
{
	client = new IClient(nullptr);
}

void test_Client_cleanup()
{
	delete client;
	client = nullptr;
}

static int instances = 0;

struct RefContainer : public IContainer {
	std::string dump() const { return "RefContainer (test)"; }

	RefContainer()  { instances++; }
	~RefContainer() { instances--; }
};

void test_Network_Channel_UserInstance()
{
	Network &n = *client->getNetwork();

	Channel *c = n.addChannel(false, ChannelIdBasic("#foobar"));
	TEST_CHECK(c != nullptr && c->cid->nameStr() == "#foobar");
	TEST_CHECK(n.addChannel(false, ChannelIdBasic("#foobar")) == c);

	instances = 0;
	{
		UserInstance *ui_d = c->addUser(UserIdBasic("Donald"));
		TEST_CHECK(c->addUser(UserIdBasic("Donald")) == ui_d);
		ui_d->set(nullptr, new RefContainer());

		TEST_CHECK(c->getUser("Mickey") == nullptr);
		UserInstance *ui_m = c->addUser(UserIdBasic("Mickey"));
		TEST_CHECK(c->getUser("miCKey") == ui_m);
		ui_m->set(nullptr, new RefContainer());

		TEST_CHECK(c->removeUser(ui_m) == true);
		// Reference is still held by Network
		TEST_CHECK(instances == 2);
		TEST_CHECK(c->getUser("Mickey") == nullptr);
		// Drop from Network -> reference gone
		TEST_CHECK(n.removeUser(ui_m) == true);
		TEST_CHECK(instances == 1);
	}

	TEST_CHECK(n.removeChannel(c) == true);
	c = nullptr;

	TEST_CHECK(n.getAllChannels().size() == 0);
	TEST_CHECK(n.getUser("Donald") == nullptr);
	TEST_CHECK(instances == 0);
}

void test_Channel(Unittest *ut)
{
	TEST_REGISTER(test_Client_setup)
	TEST_REGISTER(test_Network_Channel_UserInstance)
	TEST_REGISTER(test_Client_cleanup)
}
