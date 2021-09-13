#pragma once

#include <string>
#include <vector>

std::string strtrim(const std::string &str);
std::vector<std::string> strsplit(const std::string &input, char delim = ' ');
std::string get_next_part(std::string &input);
bool is_yes(std::string what);
