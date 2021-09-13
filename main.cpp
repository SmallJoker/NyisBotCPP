#include "core/logger.h"
#include "core/client.h"
#include "core/connection.h"
#include "core/module.h"
#include "core/settings.h"
#include "tests/test.h"

static Client *s_cli = nullptr;

void exit_main()
{
	LOG("Destructing..");
	delete s_cli;
	SLEEP_MS(100);
}

extern "C" {

DLL_EXPORT int main()
{
	LOG("Startup");

	if (0) {
		Unittest test;
		if (!test.runTests())
			exit(EXIT_FAILURE);
	}

	if (0) {
		Connection con("http://example.com", 80);

		// Demo
		std::string what("GET / HTTP/1.0\r\nHost: example.com\r\n\r\n");
		con.send(what);

		//WAIT_MS(1000);
		getchar();
	}

	Settings settings_ro("config/main.defaults.conf", nullptr);
	Settings settings_rw("config/main.user.conf", &settings_ro);
	settings_ro.syncFileContents();
	settings_rw.syncFileContents();

	atexit(exit_main);

	s_cli = new Client(&settings_rw);
	s_cli->getModuleMgr()->loadModules();
	//exit(1);
	s_cli->initialize();
	s_cli->send("PING server");

	while (s_cli->run()) {
		s_cli->processTodos();
		SLEEP_MS(20);
	}

	delete s_cli;
	s_cli = nullptr;
	return 0;
}

}
