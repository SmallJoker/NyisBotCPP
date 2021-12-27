#pragma once

#include <string>
#include <vector>

std::string strtrim(const std::string &str);
std::vector<std::string> strsplit(const std::string &input, char delim = ' ');

// Split off the next space-separated word from input
std::string get_next_part(std::string &input);

// 'true' on explicit "positive" values, 'false' otherwise
bool is_yes(std::string what);
// Case-insensitive string search
size_t strfindi(std::string haystack, std::string needle);

int get_random();

uint32_t hashELF32(const char *str, size_t length);

// IRC-specific: Processes modifiers like "+abc" and "-acd"
void apply_user_modes(char *buf, const std::string &modifier);

enum IRC_Color {
	IC_WHITE,  IC_BLACK,       IC_BLUE,   IC_GREEN,
	IC_RED,    IC_MAROON,      IC_PURPLE, IC_ORANGE,
	IC_YELLOW, IC_LIGHT_GREEN, IC_TEAL,   IC_CYAN,
	IC_LIGHT_BLUE, IC_MAGENTA, IC_GRAY,   IC_LIGHT_GRAY
};

std::string colorize_string(const std::string &str, IRC_Color color);

// Base64 utilities

std::string base64encode(const void *data, size_t len);
std::string base64decode(const void *data, size_t len);



#if 0
// Design idea.
// Catch: Does not support other client types.

class FormatHelper {
public:
	typedef uint16_t FormatType;
	enum {
		FT_BOLD   = 0x0001,
		FT_ITALIC = 0x0002,
		FT_ALL    = 0xFFFF
	};

	FormatHelper(std::ostream &os, IClient *type) :
		m_os(&os)
	{}

	~FormatHelper()
	{
		end(FT_ALL);
	}

	void begin(FormatType flags)
	{
		flags &= ~m_flags; // Already set
		if (flags & FT_BOLD) {
			*m_os << "\x02";
		}
	}

	void end(FormatType flags)
	{
		flags &= m_flags; // Already removed
		if (flags & FT_BOLD) {
			*m_os << "\x02";
		}
	}

private:
	std::ostream *m_os;
	FormatType m_flags;
};
#endif
