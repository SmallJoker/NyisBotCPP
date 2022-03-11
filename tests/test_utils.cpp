#include "test.h"
#include "../core/utils.h"

void test_Utils_strops()
{
	const char *demo = " foo  bar covfefe   ";
	TEST_CHECK(strtrim(demo) == "foo  bar covfefe");
	TEST_CHECK(strtrim(" ") == "");
	TEST_CHECK(strtrim("") == "");

	auto parts = strsplit(demo);
	TEST_CHECK(parts.size() == 3);
	TEST_CHECK(parts[0] == "foo");
	TEST_CHECK(parts[1] == "bar");
	TEST_CHECK(parts[2] == "covfefe");

	std::string leftover(demo);
	std::string part = get_next_part(leftover);
	TEST_CHECK(part == "foo");
	part = get_next_part(leftover);
	TEST_CHECK(part == "bar");
	TEST_CHECK(leftover == "covfefe   ");

	leftover = "foo baz";
	part = get_next_part(leftover);
	TEST_CHECK(part == "foo");
	TEST_CHECK(leftover == "baz");
}

void test_Utils_irc_stuff()
{
	std::string buf("    ");
	apply_user_modes(&buf[0], "+abcd");
	TEST_CHECK(buf == "abcd");
	apply_user_modes(&buf[0], "-bd");
	TEST_CHECK(buf == "a c ");

	// Colorize requires a client instance
	/*buf = " colorize me ";
	std::string normal("\x03");
	TEST_CHECK(colorize_string(buf, IC_LIGHT_GREEN) == normal + "09 colorize me \x0F");*/
}

void test_Utils_base64()
{
	std::string data1 = "I";
	std::string data2 = "SQ==";
	TEST_CHECK(base64encode(&data1[0], data1.size()) == data2);
	TEST_CHECK(base64decode(&data2[0], data2.size()) == data1);
}

void test_Utils(Unittest *ut)
{
	TEST_REGISTER(test_Utils_strops)
	TEST_REGISTER(test_Utils_irc_stuff)
	TEST_REGISTER(test_Utils_base64)
}
