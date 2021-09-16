#include "core/args_parser.h"
#include "core/logger.h"
#include "core/client_irc.h" // subject to change
#include "core/connection.h"
#include "core/module.h"
#include "core/settings.h"
#include "tests/test.h"

static IClient *s_cli = nullptr;

void exit_main()
{
	LOG("Shutting down...");
	delete s_cli;
	delete g_logger;
	SLEEP_MS(100);
}

extern "C" {

DLL_EXPORT int main(int argc, char *argv[])
{
	g_logger = new Logger();

	bool run_client = true;
	bool run_unittest = false;
	bool load_modules = true;
	bool verbose = false;
	std::string logfile("debug.txt");
	std::string config_type("user");

	CLIArg("config",     &config_type);
	CLIArg("no-modules", &load_modules);
	CLIArg("logfile",    &logfile);
	CLIArg("quicktest",  &run_client);
	CLIArg("unittest",   &run_unittest);
	CLIArg("verbose",    &verbose);
	CLIArg::parseArgs(argc, argv);

	g_logger->setupFile(logfile.c_str());
	g_logger->setLogLevels(verbose ? LL_VERBOSE : LL_NORMAL, LL_NORMAL);

	LOG("Startup");

	if (run_unittest) {
		Unittest test;
		if (!test.runTests())
			exit(EXIT_FAILURE);
		exit(EXIT_SUCCESS);
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
	Settings settings_rw("config/main." + config_type + ".conf", &settings_ro);
	settings_ro.syncFileContents();
	settings_rw.syncFileContents();

	atexit(exit_main);

	s_cli = new ClientIRC(&settings_rw);
	if (load_modules) {
		if (!s_cli->getModuleMgr()->loadModules())
			exit(EXIT_FAILURE);
	}

	if (run_client) {
		s_cli->initialize();

		while (s_cli->run()) {
			s_cli->processTodos();
			SLEEP_MS(20);
		}
	}
	return 0;
}

}
