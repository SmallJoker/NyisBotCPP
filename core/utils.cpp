#include "utils.h"

// Shameless copy
std::string strtrim(const std::string &str)
{
	size_t front = 0;

	while (std::isspace(str[front]))
		++front;

	size_t back = str.size();
	while (back > front && std::isspace(str[back - 1]))
		--back;

	return str.substr(front, back - front);
}

std::vector<std::string> strsplit(const std::string &input, char delim)
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

std::string get_next_part(std::string &input)
{
	auto pos = input.find(' ');
	std::string value;
	if (pos == std::string::npos) {
		std::swap(value, input);
		return value;
	}

	value = input.substr(0, pos);
	input = input.substr(pos + 1);
	return value;
}

bool is_yes(std::string what)
{
	what = strtrim(what);
	int num = 0;
	sscanf(what.c_str(), "%i", &num);
	if (num > 0)
		return true;

	for (char &c : what)
		c = tolower(c);

	return (what == "true" || what == "yes" || what == "on" || what == "enabled");
}
