
#include "test.h"
#include "../core/channel.h"
#include "../core/client.h"
#include "../core/logger.h"
#include "../core/settings.h"
#include "../core/utils.h"

static Client *client = nullptr;

void test_Client_setup()
{
	client = new Client(nullptr);
}

void test_Client_cleanup()
{
	delete client;
	client = nullptr;
}

static int instances = 0;

struct RefContainer : public IContainer {
	RefContainer()  { instances++; }
	~RefContainer() { instances--; }
};

void test_Network_Channel_UserInstance()
{
	Network &n = *client->getNetwork();

	Channel *c = n.getOrCreateChannel("#foobar");
	TEST_CHECK(c != nullptr && c->getName() == "#foobar");
	TEST_CHECK(n.getOrCreateChannel("#foobar") == c);

	instances = 0;
	{
		UserInstance *ui_d = c->addUser("Donald");
		TEST_CHECK(c->addUser("Donald") == ui_d);
		ui_d->data->add(nullptr, new RefContainer());

		TEST_CHECK(c->getUser("Mickey") == nullptr);
		UserInstance *ui_m = c->addUser("Mickey");
		ui_m->data->add(nullptr, new RefContainer());

		TEST_CHECK(c->removeUser(ui_m) == true);
		TEST_CHECK(instances == 2);
		TEST_CHECK(c->getUser("Mickey") == nullptr);
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
