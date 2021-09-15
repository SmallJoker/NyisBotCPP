#include "test.h"
#include "../core/logger.h"
#include <iostream>

Unittest::Unittest()
{
	// === Register tests ====
	void test_Utils(Unittest *t);
	test_Utils(this);
	void test_Chatcommand(Unittest *t);
	test_Chatcommand(this);
	void test_Module(Unittest *t);
	test_Module(this);
	void test_Settings(Unittest *t);
	test_Settings(this);
	// Channel depends on Containers (Module)
	void test_Channel(Unittest *t);
	test_Channel(this);
}


void Unittest::addTest(const char *name, func_void_t func)
{
	m_list.push_back({ name, func });
}

int Unittest::num_checked = 0;
int Unittest::num_passed = 0;

bool Unittest::runTests()
{
	num_checked = 0;
	num_passed = 0;

	std::cout << "\n===> Unittests started\n" << std::endl;

	for (const Entry &test : m_list) {
		LOG(test.name);

		try {
			test.func();
		} catch (std::string e) {
			std::cout << "\tFAIL: " << e << std::endl;
		} catch (std::exception e) {
			std::cout << "\tFAIL: " << e.what() << std::endl;
		}
	}
	std::cout << "\n===> Done. " << num_passed << " / " << num_checked << " passed!\n" << std::endl;

	return num_passed == num_checked;
}
