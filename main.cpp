#include "args_parser.h"
#include "logger.h"
#include "client/client_discord.h"
#include "client/client_irc.h"
#include "client/client_tui.h"
#include "connection.h"
#include "module.h"
#include "settings.h"
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
	atexit(exit_main);
	g_logger = new Logger();

	bool run_client = true;
	bool run_unittest = false;
	bool no_modules = false;
	bool no_random = false;
	bool verbose = false;
	std::string logfile("debug.txt");
	std::string config_type("user");

	CLIArg("config",     &config_type);
	CLIArg("no-modules", &no_modules);
	CLIArg("no-random",  &no_random);
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

	settings_ro = new Settings("config/main.defaults.conf", nullptr);
	settings_rw = new Settings("config/main." + config_type + ".conf", settings_ro);
	settings_ro->syncFileContents();
	if (!settings_rw->syncFileContents())
		exit(EXIT_FAILURE);


	if (!no_random)
		std::srand(std::time(nullptr));

	cstr_t &client_type = settings_rw->get("client.type");
	if (client_type == "irc")
		s_cli = new ClientIRC(settings_rw);
	else if (client_type == "tui")
		s_cli = new ClientTUI(settings_rw);
	else if (client_type == "discord")
		s_cli = new ClientDiscord(settings_rw);

	ASSERT(s_cli, "client.type setting value is not supported.");

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
