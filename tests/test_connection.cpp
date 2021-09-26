#include "test.h"
#include "connection.h"
#include "logger.h"

void test_Connection_Stream()
{
	Connection *con = Connection::createStream("http://example.com", 80);
	con->connect();

	// Demo
	std::string what("GET / HTTP/1.0\r\nHost: example.com\r\n\r\n");
	con->send(what);

	std::string *first = nullptr;
	while (!first) {
		sleep_ms(100);
		first = con->popRecv();
	}

	TEST_CHECK(first->rfind("HTTP/1.0 200 OK") != std::string::npos);

	delete con;
}

void test_Connection_HTTP_GET()
{
	Connection *con = Connection::createHTTP("GET", "https://example.com");
	con->connect();

	std::string *first = nullptr;
	while (!first) {
		sleep_ms(100);
		first = con->popRecv();
	}

	TEST_CHECK(first->rfind("<!doctype html>") != std::string::npos);

	delete con;
}

void test_Connection(Unittest *ut)
{
	TEST_REGISTER(test_Connection_Stream)
	TEST_REGISTER(test_Connection_HTTP_GET)
}
