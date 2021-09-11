#pragma once

#include <mutex>
#include <string>
#include <vector>

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

inline static std::vector<std::string> strsplit(cstr_t &input, char delim)
{
	std::vector<std::string> parts;

	for (size_t pos = 0; pos < input.size(); ) {
		size_t delim_pos = input.find(delim, pos);
		if (delim_pos == std::string::npos)
			delim_pos = input.size();

		if (delim_pos == pos)
			continue; // Ignore empty strings

		parts.emplace_back(input.substr(pos, delim_pos - pos));
		pos = delim_pos + 1;
	}

	return parts;
}
