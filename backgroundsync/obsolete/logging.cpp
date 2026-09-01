#include "stdafx.h"
#include "logging.h"
#include "util.h"
#include "winutil.h"
#include "arguments.h"

using std::string;

Logging& Logging::instance()
{
	static Logging obj;
	return obj;
}

void Logging::init() {
	const Arguments& args = Arguments::instance();
    setLevel(args.logLevel());
    string logFilename=args.logFilename();
    if (logFilename.empty()) {
        logFilename="/tmp/backgroundsync.log";
    }
	if (loglevel_ != Loglevel::OFF) {
        os_ = std::make_unique<std::ofstream>(logFilename, std::ios::out | std::ios::app);
        (*os_) << "--------------------------------------------------------------------------------------" << std::endl;
        (*os_) << util::currentDateTime() << " program start" << std::endl;
        (*os_) << "--------------------------------------------------------------------------------------" << std::endl;
	}
}

void Logging::setLevel(const std::string& level)
{
	const string llevel = util::toupper(level);
	if (llevel == "TRACE")
		loglevel_ = Loglevel::TRACE;
	else if (llevel == "DEBUG")
		loglevel_ = Loglevel::DEBUG;
	else if (llevel == "INFO")
		loglevel_ = Loglevel::INFO;
	else if (llevel == "WARN")
		loglevel_ = Loglevel::WARN;
	else if (llevel == "ERROR")
		loglevel_ = Loglevel::ERROR_;
	else if (llevel == "FATAL")
		loglevel_ = Loglevel::FATAL;
}
