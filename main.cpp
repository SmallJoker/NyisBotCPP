#include "core/logger.h"
#include "core/connection.h"

bool runUnittests();

int main()
{
	LOG("Startup");
	if (!runUnittests())
		exit(EXIT_FAILURE);

	//Connection con("http://example.com", 80);

	return 0;
}
