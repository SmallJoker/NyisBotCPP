
#include "../core/logger.h"
#include "../core/settings.h"
#include <cstdio>
#include <fstream>
#include <exception>

#define CHECK(expr) \
	if (!(expr)) { \
		throw std::string("Failed: " #expr); \
	}

typedef void (*func_t)();

struct TestEntry {
	const char *name;
	func_t func;
};


// ========= Settings =========

std::string settings_test_data1 =
	"key_t = $$ text value121.s\n"
	"key_i = 3\n"
	"unit.foobar = abc abc\n"
	"unit.baz = nyan\n"
	"random = true";

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

void test_Settings_read()
{
	std::string filename(std::tmpnam(nullptr));
	{
		std::ofstream of(filename);
		of << settings_test_data1;
	}

	{
		Settings s(filename);
		s.syncFileContents();
		CHECK(s.get("key_i") == "3");
		SettingsTestBoolean my_bool;
		CHECK(s.get("random", &my_bool) == true);
		CHECK(my_bool.value == true);
		s.get("key_t", &my_bool);
		CHECK(my_bool.value == false);
	}
	{
		Settings s(filename, nullptr, "unit");
		s.syncFileContents();
		CHECK(s.get("key_i") == "");
		CHECK(s.get("foobar") == "abc abc");
		
	}

	std::remove(filename.c_str());
}

void test_Settings_write()
{
	
}

void test_Settings_update()
{
	
}

bool runUnittests()
{
#define REG(func) \
	{ #func, func }

	TestEntry tests[] = {
		REG(test_Settings_read),
		REG(test_Settings_write),
		REG(test_Settings_update),
		{ nullptr, nullptr }
	};
#undef REG

	int num_ok = 0,
		num_run = 0;

	std::cout << "\n===> Unittests started\n" << std::endl;
	for (TestEntry *current_test = tests; current_test->func; ++current_test) {
		LOG(current_test->name);

		try {
			current_test->func();
			num_ok++;
		} catch (std::string e) {
			std::cout << "\tFAIL: " << e << std::endl;
		} catch (std::exception e) {
			std::cout << "\tFAIL: " << e.what() << std::endl;
		}
		num_run++;
	}
	std::cout << "\n===> Done. " << num_ok << " / " << num_run << " passed!\n" << std::endl;

	return num_run == num_ok;
}
