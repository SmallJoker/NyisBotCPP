#include "test.h"
#include "connection.h"
#include "logger.h"
#include <memory>
#include <picojson.h>

void test_Connection_Stream()
{
	std::unique_ptr<Connection> con(Connection::createStream("http://example.com", 80));
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

	delete first;
}

void test_Connection_HTTP_GET()
{
	std::unique_ptr<Connection> con(Connection::createHTTP("GET", "https://example.com"));
	con->connect();

	std::string *first = nullptr;
	while (!first) {
		sleep_ms(100);
		first = con->popRecv();
	}

	TEST_CHECK(first->rfind("<!doctype html>") != std::string::npos);

	delete first;
}

void test_Connection_HTTP_REST()
{
	std::unique_ptr<Connection> con(Connection::createHTTP("PUT", "https://jsonplaceholder.typicode.com/posts/1"));
	// Connection: Send
	con->addHTTP_Header("Content-Type: application/json; charset=UTF-8");
	con->addHTTP_Header("Accept: application/json");

	picojson::object json1;
	{
		// Add POST data
		std::string expected("{\"body\":\"bar\",\"id\":1,\"title\":\"foo\",\"userId\":100}");
		json1["id"]     = picojson::value(1.0);
		json1["title"]  = picojson::value("foo");
		json1["body"]   = picojson::value("bar");
		json1["userId"] = picojson::value(100.0);
		TEST_CHECK(picojson::value(json1).serialize() == expected);

		con->enqueueHTTP_Send(picojson::value(json1).serialize());
	}

	con->connect();

	// Connection: Receive
	std::unique_ptr<std::string> contents(con->popAll());
	TEST_CHECK(contents != nullptr);

	picojson::value json2;
	std::string err = picojson::parse(json2, *contents);
	TEST_CHECK(err.empty() == true);

	TEST_CHECK(picojson::value(json1) == json2);
}

void test_Connection(Unittest *ut)
{
	TEST_REGISTER(test_Connection_Stream)
	TEST_REGISTER(test_Connection_HTTP_GET)
	TEST_REGISTER(test_Connection_HTTP_REST)
}
