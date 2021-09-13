#include "logger.h"
#include <fstream>
#include <iomanip> // std::put_time
#include <time.h> // sleep_ms

void sleep_ms(long long ms)
{
	timespec duration {
		.tv_sec = 0,
		.tv_nsec = ms * 1000 * 1000,
	};
	nanosleep(&duration, nullptr);
}

void write_timestamp(std::ostream *os)
{
	std::time_t time = std::time(nullptr);
	std::tm *tm = std::localtime(&time);
	*os << std::put_time(tm, "[%T]");
}

void write_datetime(std::ostream *os)
{
	std::time_t time = std::time(nullptr);
	std::tm *tm = std::localtime(&time);
	*os << std::put_time(tm, "%c");
}

Logger::Logger(const char *filename)
{
	std::ofstream *os = new std::ofstream(filename, std::ios::out | std::ios::trunc);
	m_file = os;

	*os << "====> Logging started: ";
	write_datetime(os);
	*os << std::endl;
}

Logger::~Logger()
{
	m_file->flush();
	delete m_file;
}
