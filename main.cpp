#include "core/args_parser.h"
#include "core/logger.h"
#include "core/client_irc.h" // subject to change
#include "core/client_tui.h" // subject to change
#include "core/connection.h"
#include "core/module.h"
#include "core/settings.h"
#include "tests/test.h"

static IClient *s_cli = nullptr;
static Settings *settings_ro = nullptr;
static Settings *settings_rw = nullptr;

void exit_main()
{
	LOG("Shutting down...");
	delete s_cli;
	delete settings_rw;
	delete settings_ro;
	delete g_logger;
	SLEEP_MS(100);
}

extern "C" {

DLL_EXPORT int main(int argc, char *argv[])
{
	g_logger = new Logger();

	bool run_client = true;
	bool run_unittest = false;
	bool no_modules = false;
	bool no_random = false;
	bool use_tui = false;
	bool verbose = false;
	std::string logfile("debug.txt");
	std::string config_type("user");

	CLIArg("config",     &config_type);
	CLIArg("no-modules", &no_modules);
	CLIArg("no-random",  &no_random);
	CLIArg("logfile",    &logfile);
	CLIArg("quicktest",  &run_client);
	CLIArg("unittest",   &run_unittest);
	CLIArg("tui",        &use_tui);
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

	settings_ro = new Settings("config/main.defaults.conf", nullptr);
	settings_rw = new Settings("config/main." + config_type + ".conf", settings_ro);
	settings_ro->syncFileContents();
	settings_rw->syncFileContents();

	atexit(exit_main);

	if (!no_random)
		std::srand(std::time(nullptr));

	if (use_tui)
		s_cli = new ClientTUI(settings_rw);
	else
		s_cli = new ClientIRC(settings_rw);

	if (!no_modules) {
		if (!s_cli->getModuleMgr()->loadModules())
			exit(EXIT_FAILURE);
	}

	if (run_client) {
		s_cli->initialize();

		while (s_cli->run()) {
			s_cli->processRequests();
			SLEEP_MS(20);
		}
	}
	return 0;
}

}
