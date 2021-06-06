#pragma once

#include <vector>
#include <exception>

#define TEST_CHECK(expr) \
	if (!(expr)) { \
		throw std::string("Failed: " #expr); \
	}

#define TEST_REGISTER(func) \
	ut->addTest(#func, func);


typedef void (*func_void_t)();


class Unittest {
public:
	Unittest();

	bool runTests();
	void addTest(const char *name, func_void_t func);

private:
	struct Entry {
		const char *name;
		func_void_t func;
	};

	std::vector<Entry> m_list;
};
