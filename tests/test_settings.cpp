
#include "test.h"
#include "../core/logger.h"
#include "../core/settings.h"
#include <fstream>

class SettingsTestBoolean : public SettingType {
public:
	bool deSerialize(cstr_t &str)
	{
		value = (str == "true");
		return true;
	}

	std::string serialize() const
	{
		return value ? "true" : "false";
	}

	bool value = false;
};

class SettingTypeString : public SettingType {
public:
	bool deSerialize(cstr_t &str)
	{
		value = str;
		return !value.empty();
	}

	std::string serialize() const
	{
		return value;
	}

	std::string value;
};


std::string settings_test_data1 =
	"key_t = $$ text value121.s\n"
	"key_i = 3\n"
	"unit.foobar = abc abc\n"
	"unit.baz = nyan\n"
	"random = true";

// After write manipulations
std::string settings_test_data2 =
	"key_t = $$ text value121.s\n"
	"key_i = 999\n"
	"unit.baz = kittens are cute\n"
	"random = true\n"
	"unit.newy = whoa!";


void test_Settings_read()
{
	std::string filename(std::tmpnam(nullptr));
	{
		std::ofstream os(filename);
		os << settings_test_data1;
	}

	{
		Settings s(filename);
		s.syncFileContents();
		TEST_CHECK(s.get("key_i") == "3");

		SettingsTestBoolean my_bool;
		TEST_CHECK(s.get("random", &my_bool) == true);
		TEST_CHECK(my_bool.value == true);

		s.get("key_t", &my_bool);
		TEST_CHECK(my_bool.value == false);
	}
	{
		Settings s(filename, nullptr, "unit");
		s.syncFileContents();
		TEST_CHECK(s.get("key_i") == "");
		TEST_CHECK(s.get("foobar") == "abc abc");
	}

	std::remove(filename.c_str());
}


void test_Settings_write()
{
	std::string filename(std::tmpnam(nullptr));
	{
		std::ofstream os(filename);
		os << settings_test_data1;
	}

	Settings sr(filename); // Reader
	sr.syncFileContents();
	{
		Settings sw(filename); // Writer
		sw.syncFileContents();

		sw.set("key_i", "999");
		sw.syncFileContents();
		sr.syncFileContents();
		TEST_CHECK(sw.get("key_i") == sr.get("key_i"));
	}
	{
		Settings sw(filename, nullptr, "unit"); // Writer
		sw.syncFileContents();

		// Update key
		sw.set("baz", "kittens are cute");
		sw.syncFileContents();
		sr.syncFileContents();
		TEST_CHECK(sw.get("baz") == sr.get("unit.baz"));

		// Key removal
		sw.remove("baz");
		sw.syncFileContents();
		sr.syncFileContents();
		SettingTypeString my_sink;
		TEST_CHECK(!sw.get("baz", &my_sink));
		TEST_CHECK(!sr.get("unit.baz", &my_sink));

		// New key
		sw.set("newy", "whoa!");
		sw.syncFileContents();
		sr.syncFileContents();
		TEST_CHECK(sr.get("unit.newy") == "whoa!");
		TEST_CHECK(sr.get("unit.newy", &my_sink));
	}

	{
		std::ifstream is(filename);
		std::string tmp, entire;
		entire.reserve(settings_test_data2.size());

		while (std::getline(is, tmp)) {
			entire.append(tmp);
			entire.append("\n");
		}
		// TODO: Fix blank lines
		LOG("\n" << trim(entire));
		TEST_CHECK(trim(entire) == settings_test_data2);
	}

	std::remove(filename.c_str());
}

void test_Settings_update()
{
	
}

void test_Settings(Unittest *ut)
{
	TEST_REGISTER(test_Settings_read)
	TEST_REGISTER(test_Settings_write)
	TEST_REGISTER(test_Settings_update)
}

