#include "core/logger.h"
#include "core/client.h"
#include "core/connection.h"
#include "tests/test.h"

int main()
{
	LOG("Startup");

	Unittest test;
	if (!test.runTests())
		exit(EXIT_FAILURE);

	if (0) {
		Connection con("http://example.com", 80);

		// Demo
		std::string what("GET / HTTP/1.0\r\nHost: example.com\r\n\r\n");
		con.send(what);

		//WAIT_MS(1000);
		getchar();
	}

	Client cli("irc.shells.org", 6669);
	cli.send("USER DummyBot_0 foo bar :Testing bot");
	cli.send("NICK DummyBot_0");
	while (cli.run());
	
	return 0;
}
