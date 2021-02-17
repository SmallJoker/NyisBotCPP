#pragma once

#include <string>
#include <mutex>

typedef std::unique_lock<std::mutex> MutexAutoLock;
typedef const std::string cstr_t;

// Shameless copy
inline static std::string trim(cstr_t &str)
{
	size_t front = 0;

	while (std::isspace(str[front]))
		++front;

	size_t back = str.size();
	while (back > front && std::isspace(str[back - 1]))
		--back;

	return str.substr(front, back - front);
}
