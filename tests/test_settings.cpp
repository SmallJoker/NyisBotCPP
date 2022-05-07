
#include "test.h"
#include "../core/logger.h"
#include "../core/settings.h"
#include <fstream>
#include <memory> // std::unique_ptr

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
	"\n"
	"key_i = 3\n"
	"unit.foobar = abc abc\n"
	"unit.baz = nyan\n"
	"random = true";

// After write manipulations
std::string settings_test_data2 =
	"key_t = $$ text value121.s\n"
	"\n"
	"key_i = 999\n"
	"unit.baz = kittens are cute\n"
	"random = true\n"
	"unit.newy = whoa!\n";


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
		TEST_CHECK(s.get("foobar") == "abc abc")
		auto keys = s.getKeys();
		TEST_CHECK(keys.size() == 2);
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
	std::unique_ptr<Settings> sr_fork(sr.fork("unit"));
	sr.syncFileContents();

	// Fork test 1: Values must be read in
	TEST_CHECK(sr_fork->getKeys().size() == 2);
	TEST_CHECK(sr_fork->get("baz") == "nyan");

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
		sw.remove("foobar");
		sw.syncFileContents();
		sr.syncFileContents();
		SettingTypeString my_sink;
		TEST_CHECK(!sw.get("foobar", &my_sink));
		TEST_CHECK(!sr.get("unit.foobar", &my_sink));

		// New key
		sw.set("newy", "whoa!");
		sw.syncFileContents();
		sr.syncFileContents();
		TEST_CHECK(sr.get("unit.newy") == "whoa!");
		TEST_CHECK(sr.get("unit.newy", &my_sink));
	}

	// Fork test 1: Value updated after write
	TEST_CHECK(sr_fork->get("baz") == "kittens are cute");

	{
		std::ifstream is(filename);
		std::string tmp, entire;
		entire.reserve(settings_test_data2.size());

		while (std::getline(is, tmp)) {
			entire.append(tmp);
			entire.append("\n");
		}
		TEST_CHECK(entire == settings_test_data2);
	}

	std::remove(filename.c_str());
}

void test_SettingType_util()
{
	std::string text("2903 C0FFEE 1.55E1 ");
	const char *pos = text.c_str();
	int64_t longval;
	float floatval;
	std::string stringval;

	TEST_CHECK(SettingType::parseS64(&pos, &longval) == true);
	TEST_CHECK(longval == 2903);
	TEST_CHECK(SettingType::parseS64(&pos, &longval, 16) == true);
	TEST_CHECK(longval == 0xC0FFEE);
	TEST_CHECK(SettingType::parseFloat(&pos, &floatval) == true);
	TEST_CHECK(std::abs(floatval - 1.55E1f) < 0.001f);
	TEST_CHECK(SettingType::parseS64(&pos, &longval) == false); // end
	{
		pos = text.c_str();
		TEST_CHECK(SettingType::parseString(&pos, &stringval, ' ') == true);
		TEST_CHECK(stringval == "2903");
		TEST_CHECK(SettingType::parseString(&pos, &stringval, '.') == true);
		TEST_CHECK(stringval == "C0FFEE 1");
		TEST_CHECK(SettingType::parseString(&pos, &stringval, '\0') == true);
		TEST_CHECK(stringval == "55E1 ");
		TEST_CHECK(SettingType::parseString(&pos, &stringval, ' ') == false);
		TEST_CHECK(pos == nullptr);
	}

	Settings::sanitizeKey(text);
	auto parts = strsplit(text, '_');
	TEST_CHECK(parts[0] == "2903C0FFEE1.55E1");
	TEST_CHECK(parts[1].size() == 8);

	{
		text = "valid_key";
		Settings::sanitizeKey(text);
		TEST_CHECK(text == "valid_key");
	}

	// Base64 data
	text = "2903 C0FFEE 1.55E1 ";
	std::string base64(base64encode(text.c_str(), text.size()));
	TEST_CHECK(base64 == "MjkwMyBDMEZGRUUgMS41NUUxIA==");
}

void test_Settings(Unittest *ut)
{
	TEST_REGISTER(test_Settings_read)
	TEST_REGISTER(test_Settings_write)
	TEST_REGISTER(test_SettingType_util)
}
