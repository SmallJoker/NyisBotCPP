
#include "test.h"
#include "../core/logger.h"
#include "../core/module.h"


void test_Module_load_unload()
{
	// Module tests
	ModuleMgr m;
	m.loadModules();
	bool ok = m.reloadModule("builtin");
	TEST_CHECK(ok == true);
	m.unloadModules();
}

void test_Module(Unittest *ut)
{
	TEST_REGISTER(test_Module_load_unload)
}
