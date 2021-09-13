
#include "test.h"
#include "../core/container.h"
#include "../core/logger.h"
#include "../core/module.h"


void test_Module_load_unload()
{
	// Module tests
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
		Containers c;
		instances = 0;

		TEST_CHECK(c.add(&c,      new DemoContainer(11)) == true);
		TEST_CHECK(c.add(nullptr, new DemoContainer(55)) == true);
		DemoContainer *dc = new DemoContainer(77);
		TEST_CHECK(c.add(&c,      dc) == false);
		delete dc;

		TEST_CHECK(instances == 2);

		dc = (DemoContainer *)c.get(&c);
		TEST_CHECK(dc != nullptr && dc->num == 11);

		TEST_CHECK(c.remove(&c) == true);
		TEST_CHECK(c.remove(&c) == false);
		TEST_CHECK(c.get(&c) == nullptr);
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
