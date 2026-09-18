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

class DailyLogCut {  // [LogCut.md#DailyLogCut]
	using TimePoint = std::chrono::system_clock::time_point;

	TimePoint	next_cut_ = (TimePoint::min)();
public:
	void operator()(TimePoint now = std::chrono::system_clock::now());
};

} // namespace AFS_Core
