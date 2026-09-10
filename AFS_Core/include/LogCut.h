#pragma once
#include "Logging.h"
#include <fstream>
#include <chrono>

namespace AFS_Core {

class LogCut {
	using TimePoint = std::chrono::system_clock::time_point;

	Logging&		log_;
	TimePoint		now_;
	std::ifstream	is_;
	std::ofstream	os_;

	bool open_files();
	void close_files();
	void cut();
	bool get_time_from_line(const std::string& line, TimePoint& tp);
public:
	LogCut();
	void operator()();
};


} // namespace AFS_Core
