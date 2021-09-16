#pragma once

#include <string>
#include <vector>

std::string strtrim(const std::string &str);
std::vector<std::string> strsplit(const std::string &input, char delim = ' ');
std::string get_next_part(std::string &input);
bool is_yes(std::string what);
int get_random();

void apply_user_modes(char *buf, const std::string &modifier);

enum IRC_Color {
	IC_WHITE,  IC_BLACK,       IC_BLUE,   IC_GREEN,
	IC_RED,    IC_MAROON,      IC_PURPLE, IC_ORANGE,
	IC_YELLOW, IC_LIGHT_GREEN, IC_TEAL,   IC_CYAN,
	IC_LIGHT_BLUE, IC_MAGENTA, IC_GRAY,   IC_LIGHT_GRAY
};

std::string colorize_string(const std::string &str, IRC_Color color);
