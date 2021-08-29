#include "core/logger.h"
#include "core/module.h"
#include "core/connection.h"
#include "tests/test.h"

int main()
{
	LOG("Startup");

	if (1) {
		// Module tests
		      ModuleMgr m;
		m.loadModules();
		bool ok = m.reloadModule("builtin");
		LOG("Reload OK? " << (int)ok);
		m.unloadModules();
		exit(0);
	}

	Unittest test;
	if (!test.runTests())
		exit(EXIT_FAILURE);

	Connection con("http://example.com", 80);

	// Demo
	std::string what("GET / HTTP/1.0\r\nHost: example.com\r\n\r\n");
	con.send(what);

	//WAIT_MS(1000);

	return 0;
}
