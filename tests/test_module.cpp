
#include "test.h"
#include "../core/container.h"
#include "../core/logger.h"
#include "../core/module.h"
#include "../core/settings.h"
#include "../client/client_tui.h"

void test_Module_load_unload()
{
	ModuleMgr m(nullptr);

	m.loadModules();
	bool ok = m.reloadModule("builtin");
	TEST_CHECK(ok == true);
	m.unloadModules();
}

static int instances = 0;

struct DemoContainer : public IContainer {
	DemoContainer(int x) { num = x; instances++; }

	~DemoContainer() { instances--; }

	std::string dump() const { return std::to_string(num); }

	int num;
};

void test_Module_Container()
{
	{
		ContainerOwner ref;
		Containers c;
		instances = 0;

		c.set(&ref,    new DemoContainer(11));
		c.set(nullptr, new DemoContainer(55));

		TEST_CHECK(instances == 2);

		DemoContainer *dc = (DemoContainer *)c.get(&ref);
		TEST_CHECK(dc != nullptr && dc->num == 11);

		TEST_CHECK(c.remove(&ref) == true);
		TEST_CHECK(c.remove(&ref) == false);
		TEST_CHECK(c.get(&ref) == nullptr);
		TEST_CHECK(instances == 1);

		dc = (DemoContainer *)c.get(nullptr);
		TEST_CHECK(dc != nullptr && dc->dump() == "55");
	}

	TEST_CHECK(instances == 0);
}

void test_Module(Unittest *ut)
{
	TEST_REGISTER(test_Module_load_unload)
	TEST_REGISTER(test_Module_Container)
}
