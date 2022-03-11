#pragma once

#include <string>
#include <vector>

class UserInstance;

std::string strtrim(const std::string &str);
std::vector<std::string> strsplit(const std::string &input, char delim = ' ');

// Split off the next space-separated word from input
std::string get_next_part(std::string &input);

// 'true' on explicit "positive" values, 'false' otherwise
bool is_yes(std::string what);
// Case-insentitive string compare
bool strequalsi(const std::string &a, const std::string &b);
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

// Base64 utilities

std::string base64encode(const void *data, size_t len);
std::string base64decode(const void *data, size_t len);


typedef uint16_t FormatType;
enum : uint16_t {
	FT_BOLD    = 0x0001,
	FT_ITALICS = 0x0002,
	FT_UNDERL  = 0x0004,
	FT_COLOR   = 0x0100,
	FT_ALL     = 0xFFFF
};

class IFormatter {
public:
	IFormatter();
	virtual ~IFormatter();

	virtual void mention(UserInstance *ui)
	{
		*m_os << std::string("(invalid)");
	}

	void begin(IRC_Color color)
	{
		if (m_flags & FT_COLOR)
			endImpl(FT_COLOR);

		m_flags |= FT_COLOR;
		beginImpl(color);
	}

	void begin(FormatType flags)
	{
		flags &= ~m_flags; // newly set flags
		flags &= ~FT_COLOR; // disallowed
		m_flags |= flags;

		beginImpl(flags);
	}

	void end(FormatType flags)
	{
		flags &= m_flags; // newly removed flags
		m_flags &= ~flags;

		endImpl(flags);
	}

	template <typename T>
	IFormatter &operator<<(const T &what)
	{
		*m_os << what;
		return *this;
	}

	std::string str() const;

protected:
	virtual void beginImpl(IRC_Color color) {}
	virtual void beginImpl(FormatType flags) {}
	virtual void endImpl(FormatType flags) {}

	std::ostream *m_os;

private:
	FormatType m_flags;
};
