#include "args_parser.h"
#include "logger.h"
#include <map>

std::map<std::string, CLIArg *> g_args;

CLIArg::CLIArg(cstr_t &pfx, bool *out) : CLIArg(pfx)
{
	m_handler = &CLIArg::parseFlag;
	m_dst.b = out;
}

CLIArg::CLIArg(cstr_t &pfx, int64_t *out) : CLIArg(pfx)
{
	m_handler = &CLIArg::parseS64;
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
	g_args.insert({"--" + pfx, this});
}

static const std::string HELPCMD("--help");

void CLIArg::parseArgs(int argc, char **argv)
{
	for (int i = 1; i < argc; ++i) {
		if (argv[i] == HELPCMD) {
			showHelp();
			exit(EXIT_SUCCESS);
		}

		auto it = g_args.find(argv[i]);
		if (it == g_args.end()) {
			WARN("Unknown argument '" << argv[i] << "'");
			continue;
		}

		bool has_argv = (it->second->m_handler != &CLIArg::parseFlag);
		bool ok = (i + 1 < argc) || !has_argv;
		if (ok) {
			// Note: argv is NULL-terminated
			ok = (it->second->*(it->second)->m_handler)(argv[i + 1]);
			if (ok && has_argv)
				i++;
		}

		if (!ok)
			WARN("Cannot parse argument '" << argv[i] << "'");
	}
	g_args.clear();
}

void CLIArg::showHelp()
{
	LoggerAssistant la(LL_NORMAL);
	la << "Available commands:\n";

	for (const auto &it : g_args) {
		const CLIArg *a = it.second;
		const char *type = "invalid";
		std::string initval;

		// Switch-case does not work ....
		if (a->m_handler == &CLIArg::parseFlag) {
			type = "flag";
		} else if (a->m_handler == &CLIArg::parseS64) {
			type = "integer";
			initval = std::to_string(*a->m_dst.l);
		} else if (a->m_handler == &CLIArg::parseFloat) {
			type = "float";
			initval = std::to_string(*a->m_dst.f);
		} else if (a->m_handler == &CLIArg::parseString) {
			type = "string";
			initval = '"' + *a->m_dst.s + '"';
		}
		la << "\t" << it.first << "\t (Type: " << type;
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

bool CLIArg::parseS64(const char *str)
{
	long long val = *m_dst.l;
	int status = sscanf(str, "%lli", &val);
	*m_dst.l = val;
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
