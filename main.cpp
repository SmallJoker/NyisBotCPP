#include "inc/logger.h"
#include "inc/connection.h"

int main()
{
	LOG("Startup");
	Connection con("http://example.com", 80);
	
    return 0;
}
