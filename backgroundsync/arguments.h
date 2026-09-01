#pragma once
#include <string>

class Arguments {
	std::string logFilename_;
    std::string loglevel_;
    bool        singleSync_=false;
	Arguments();
public:
	void init(int argc, char* argv[]);
    static Arguments& instance();
	std::string logFilename() const { return logFilename_; }
    std::string logLevel() const { return loglevel_; }
    bool singleSync() const { return singleSync_; }
};

