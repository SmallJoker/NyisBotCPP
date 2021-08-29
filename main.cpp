#include "core/logger.h"
#include "core/module.h"
#include "core/connection.h"
#include "tests/test.h"

int main()
{
	LOG("Startup");

	if (1) {
		// Module tests
		BotManager m;
		m.reloadModules();
		exit(0);
	}

	Unittest test;
	if (!test.runTests())
		exit(EXIT_FAILURE);

	Connection con("http://example.com", 80);

	return 0;
}
