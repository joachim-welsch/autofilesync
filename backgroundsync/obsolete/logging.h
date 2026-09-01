#pragma once
#include <string>
#include <fstream>
#include <memory>
#include <sstream>
#include "winutil.h"

enum class Loglevel { OFF=0, TRACE, DEBUG, INFO, WARN, ERROR_, FATAL };

class Logging {
	Loglevel loglevel_ = Loglevel::OFF;
	std::unique_ptr<std::ofstream> os_;
public:
	static Logging& instance();
	void init();
	void setLevel(const std::string&);
	template <typename ...Args>
    void trace(/*int line, const char* fileName, */ Args&& ...args) {
        if (loglevel_ != Loglevel::TRACE)
			return;
		std::ostringstream stream;
        //stream << fileName << "(" << line << ") : ";
        stream << util::currentDateTime() << " | ";
        (stream << ... << std::forward<Args>(args));
        (*os_) << stream.str() << std::endl;
	}
    template <typename ...Args>
    void info(/*int line, const char* fileName, */ Args&& ...args) {
        if (loglevel_>Loglevel::OFF && loglevel_ <= Loglevel::INFO)
            return;
        std::ostringstream stream;
        //stream << fileName << "(" << line << ") : ";
        stream << util::currentDateTime() << " | ";
        (stream << ... << std::forward<Args>(args));
        (*os_) << stream.str() << std::endl;
    }
};

//#define TRACE(...) Logging::instance().trace(__LINE__, __FILE__, __VA_ARGS__)
#define TRACE(...) Logging::instance().trace(__VA_ARGS__)
#define INFO(...) Logging::instance().info(__VA_ARGS__)
