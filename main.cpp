#include "core/logger.h"
#include "core/connection.h"
#include "tests/test.h"

int main()
{
	LOG("Startup");
	Unittest test;
	if (!test.runTests())
		exit(EXIT_FAILURE);

	//Connection con("http://example.com", 80);

	return 0;
}
