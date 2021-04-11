#pragma once

#include <iostream> // cout

#define LOG(str) \
	std::cout << "\e[0;36m" << __PRETTY_FUNCTION__ << "\e[0m: " << str << std::endl

#define WARN(str) \
	std::cout << "\e[1;33m" << __PRETTY_FUNCTION__ << "\e[0m: " << str << std::endl

#define ERROR(str) \
	std::cout << "\e[0;31m" << __PRETTY_FUNCTION__ << "\e[0m: " << str << std::endl; \
	exit(EXIT_FAILURE)

#define ASSERT(expr, msg) \
	if (!(expr)) { \
		std::cout << "\e[0;31m" << __PRETTY_FUNCTION__ << "\e[0m: Assertion " \
			<< #expr << ": " << msg << std::endl; \
		throw std::string("Assertion failed"); \
	}
