#pragma once

#include <iostream> // cout

#define VERBOSE(str) \
	std::cout << "\e[1;30m" << __PRETTY_FUNCTION__ << "\e[0m: " << str << std::endl

#define LOG(str) \
	std::cout << "\e[0;36m" << __PRETTY_FUNCTION__ << "\e[0m: " << str << std::endl

#define WARN(str) \
	std::cout << "\e[1;33m" << __PRETTY_FUNCTION__ << "\e[0m: " << str << std::endl

#define ERROR(str) \
	std::cout << "\e[0;31m" << __PRETTY_FUNCTION__ << "\e[0m: " << str << std::endl

#define ASSERT(expr, msg) \
	if (!(expr)) { \
		std::cout << "\e[0;31m" << __PRETTY_FUNCTION__ << "\e[0m: Assertion " \
			<< #expr << ": " << msg << std::endl; \
		throw std::string("Assertion failed"); \
	}


#define SLEEP_MS(ms) \
	std::this_thread::sleep_for(std::chrono::milliseconds((ms)));
