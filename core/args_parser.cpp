#include "args_parser.h"
#include "logger.h"
#include <vector>

std::vector<CLIArg *> g_args;

CLIArg::CLIArg(cstr_t &pfx, bool *out) : CLIArg(pfx)
{
	m_handler = &CLIArg::parseFlag;
	m_dst.b = out;
}

CLIArg::CLIArg(cstr_t &pfx, long *out) : CLIArg(pfx)
{
	m_handler = &CLIArg::parseLong;
	m_dst.l = out;
}

CLIArg::CLIArg(cstr_t &pfx, float *out) : CLIArg(pfx)
{
	m_handler = &CLIArg::parseFloat;
	m_dst.f = out;
}

CLIArg::CLIArg(cstr_t &pfx, std::string *out) : CLIArg(pfx)
{
	m_handler = &CLIArg::parseString;
	m_dst.s = out;
}

CLIArg::CLIArg(cstr_t &pfx)
{
	m_prefix = "--" + pfx;
	g_args.emplace_back(this);
}

static const std::string HELPCMD("--help");

void CLIArg::parseArgs(int argc, char **argv)
{
	for (int i = 0; i < argc; ++i) {
		for (CLIArg *a : g_args) {
			if (argv[i] == HELPCMD) {
				showHelp();
				exit(EXIT_SUCCESS);
			}

			if (a->m_prefix != argv[i])
				continue;

			if (a->m_handler == &CLIArg::parseFlag) {
				// Single
				(a->*a->m_handler)(nullptr);
				break;
			}

			if (i + 1 < argc && (a->*a->m_handler)(argv[i + 1]))
				++i;
			else
				WARN("Cannot parse argument '" << argv[i] << "'");

			break;
		}
	}
	g_args.clear();
}

void CLIArg::showHelp()
{
	LoggerAssistant la(LL_NORMAL);
	la << "Available commands:\n";

	for (CLIArg *a : g_args) {
		const char *type = "invalid";
		std::string initval;

		// Switch-case does not work ....
		if (a->m_handler == &CLIArg::parseFlag) {
			type = "flag";
		} else if (a->m_handler == &CLIArg::parseLong) {
			type = "integer";
			initval = std::to_string(*a->m_dst.l);
		} else if (a->m_handler == &CLIArg::parseFloat) {
			type = "float";
			initval = std::to_string(*a->m_dst.f);
		} else if (a->m_handler == &CLIArg::parseString) {
			type = "string";
			initval = '"' + *a->m_dst.s + '"';
		}
		la << "\t" << a->m_prefix << "\t (Type: " << type;
		if (!initval.empty())
			la << ", Default: " << initval;
		la << ")\n";
	}
}

bool CLIArg::parseFlag(const char *str)
{
	*m_dst.b ^= true;
	return 1;
}

bool CLIArg::parseLong(const char *str)
{
	int status = sscanf(str, "%li", m_dst.l);
	return status == 1;
}

bool CLIArg::parseFloat(const char *str)
{
	int status = sscanf(str, "%f", m_dst.f);
	return status == 1;
}

bool CLIArg::parseString(const char *str)
{
	if (!str[0])
		return false;

	m_dst.s->assign(str);
	return true;
}
