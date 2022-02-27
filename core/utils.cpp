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

bool strequalsi(const std::string &a, const std::string &b)
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (tolower(a[i]) != tolower(b[i]))
			return false;
	return true;
}

size_t strfindi(std::string haystack, std::string needle)
{
	for (char &c : haystack)
		c = tolower(c);
	for (char &c : needle)
		c = tolower(c);
	return haystack.rfind(needle);
}

int get_random()
{
	// Seeded by main()
	return rand();
}

uint32_t hashELF32(const char *str, size_t length)
{
	// Source: http://www.partow.net/programming/hashfunctions/index.html
	uint32_t hash = 0, x = 0;

	for (size_t i = 0; i < length; ++str, ++i) {
		hash = (hash << 4) + (*str);
		if ((x = hash & 0xF0000000L) != 0) {
			hash ^= (x >> 24);
		}
		hash &= ~x;
	}

	return hash;
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

// Base64 encoder and decoder by polfosol
// https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c/37109258#37109258

static const char *B64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const int B64index[256] = {
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  62, 63, 62, 62, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0,  0,  0,  0,  0,  0,
	0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0,  0,  0,  0,  63,
	0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

std::string base64encode(const void *data, size_t len)
{
	std::string result((len + 2) / 3 * 4, '=');
	unsigned char *p = (unsigned  char *)data;
	char *str = &result[0];
	size_t j = 0, pad = len % 3;
	const size_t last = len - pad;

	for (size_t i = 0; i < last; i += 3)  {
		int n = (int)p[i] << 16 | (int)p[i + 1] << 8 | p[i + 2];
		str[j++] = B64chars[n >> 18];
		str[j++] = B64chars[n >> 12 & 0x3F];
		str[j++] = B64chars[n >> 6 & 0x3F];
		str[j++] = B64chars[n & 0x3F];
	}
	if (pad) {  /// Set padding
		int n = --pad ? (int)p[last] << 8 | p[last + 1] : p[last];
		str[j++] = B64chars[pad ? n >> 10 & 0x3F : n >> 2];
		str[j++] = B64chars[pad ? n >> 4 & 0x03F : n << 4 & 0x3F];
		str[j++] = pad ? B64chars[n << 2 & 0x3F] : '=';
	}
	return result;
}

std::string base64decode(const void *data, size_t len)
{
	if (len == 0) return "";

	unsigned char *p = (unsigned char *)data;
	size_t j = 0,
		pad1 = len % 4 || p[len - 1] == '=',
		pad2 = pad1 && (len % 4 > 2 || p[len - 2] != '=');
	const size_t last = (len - pad1) / 4 << 2;
	std::string result(last / 4 * 3 + pad1 + pad2, '\0');
	unsigned char *str = (unsigned char *)&result[0];

	for (size_t i = 0; i < last; i += 4) {
		int n = B64index[p[i]] << 18 | B64index[p[i + 1]] << 12 | B64index[p[i + 2]] << 6 | B64index[p[i + 3]];
		str[j++] = n >> 16;
		str[j++] = n >> 8 & 0xFF;
		str[j++] = n & 0xFF;
	}
	if (pad1) {
		int n = B64index[p[last]] << 18 | B64index[p[last + 1]] << 12;
		str[j++] = n >> 16;
		if (pad2) {
			n |= B64index[p[last + 2]] << 6;
			str[j++] = n >> 8 & 0xFF;
		}
	}
    return result;
}
