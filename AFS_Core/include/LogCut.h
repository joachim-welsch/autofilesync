#pragma once
#include "Logging.h"
#include <fstream>
#include <chrono>

namespace AFS_Core {

class LogCut {
	using time_point = std::chrono::system_clock::time_point;

	Logging&		log_;
	time_point		now_;
	std::ifstream	is_;
	std::ofstream	os_;

	bool openFiles();
	void closeFiles();
	void cut();
	bool getTimeFromLine(const std::string& line, time_point& tp);
public:
	LogCut();
	void operator()();
};


} // namespace AFS_Core
