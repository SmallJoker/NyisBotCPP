#pragma once

#include "types.h"

class CLIArg {
public:
	CLIArg(cstr_t &pfx, bool *out);
	CLIArg(cstr_t &pfx, int64_t *out);
	CLIArg(cstr_t &pfx, float *out);
	CLIArg(cstr_t &pfx, std::string *out);

	static void parseArgs(int argc, char **argv);

private:
	CLIArg(cstr_t &pfx);
	static void showHelp();

	bool parseFlag(const char *str);
	bool parseS64(const char *str);
	bool parseFloat(const char *str);
	bool parseString(const char *str);

	union {
		bool *b;
		int64_t *l;
		float *f;
		std::string *s;
	} m_dst;

	bool (CLIArg::*m_handler)(const char *str);
};
