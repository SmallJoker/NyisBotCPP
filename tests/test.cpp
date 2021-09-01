#include "test.h"
#include "../core/logger.h"

Unittest::Unittest()
{
	// === Register tests ====
	void test_Settings(Unittest *t);
	test_Settings(this);
	void test_Module(Unittest *t);
	test_Module(this);
}


void Unittest::addTest(const char *name, func_void_t func)
{
	m_list.push_back({ name, func });
}


bool Unittest::runTests()
{
	int num_ok = 0,
		num_run = 0;

	std::cout << "\n===> Unittests started\n" << std::endl;

	for (const Entry &test : m_list) {
		LOG(test.name);

		try {
			test.func();
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
