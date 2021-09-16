#include "utils.h"
#include <string.h>

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

		if (delim_pos == pos) {
			pos++;
			continue; // Ignore empty strings
		}

		parts.emplace_back(input.substr(pos, delim_pos - pos));
		pos = delim_pos + 1;
	}

	return parts;
}

std::string get_next_part(std::string &input)
{
	char *pos_a = &input[0];
	while (*pos_a && std::isspace(*pos_a))
		pos_a++;

	char *pos_b = pos_a;
	while (*pos_b && !std::isspace(*pos_b))
		pos_b++;

	if (!*pos_a) {
		// End reached
		input.clear();
		return "";
	}

	// Split into two parts
	bool ended = !*pos_b;
	*pos_b = '\0';

	std::string value(pos_a); // Until terminator
	if (ended)
		input.clear();
	else
		input = std::string(pos_b + 1);
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

int get_random()
{
	static thread_local bool init = false;
	if (!init) {
		srand(time(nullptr));
		init = true;
	}

	return rand();
}

void apply_user_modes(char *buf, const std::string &modifier)
{
	bool add = (modifier[0] != '-');
	for (char c : modifier) {
		if (c == '+' || c == '-')
			continue;

		char *pos = strchr(buf, c);
		if (add == (pos != nullptr))
			continue; // Nothing to changed

		if (add) {
			pos = strchr(buf, ' ');
			if (pos)
				*pos = c;
		} else {
			*pos = ' ';
		}
	}
}

std::string colorize_string(const std::string &str, IRC_Color color)
{
	std::string out(str.size() + 2 + 2 + 1, '\0');
	int len = sprintf(&out[0], "\x03%02i%s\x0F", (int)color, str.c_str());
	out.resize(len);
	return out;
}
