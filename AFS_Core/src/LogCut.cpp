#include "pch.h"
#include "LogCut.h"
#include <string>
#include <sstream>
#include <ctime>
#include <filesystem>
#ifdef _UNIX
    #include "StrUtil.h"
#endif

namespace AFS_Core {

namespace fs = std::filesystem;

#ifdef _UNIX
    using util::wstring_to_string;
#endif

using std::chrono::system_clock;

LogCut::LogCut()
    : log_{ Logging::instance() }
    , now_{ system_clock::now() }
{}

static auto nativePath(const std::wstring& path)
{
#ifdef _UNIX
    return wstring_to_string(path);
#else
	return path;
#endif
}

bool LogCut::openFiles()
{
	log_.closeFile();
	const std::wstring pathName = log_.getLogFilePathName();
	is_.open(nativePath(pathName), std::ios::in);
	// The temp file is created only once the log itself opened: on the early
	// return the caller skips closeFiles(), and with it the rename that turns
	// the temp file back into the log - creating it eagerly left an orphaned
	// ".tmp" behind on every start without an existing log file.
	if (!is_.is_open())
		return false;
	os_.open(nativePath(pathName + L".tmp"), std::ios::out);
	return os_.is_open();
}

void LogCut::closeFiles()
{
	is_.close();
	os_.close();
	fs::rename(log_.getLogFilePathName() + L".tmp", log_.getLogFilePathName()); //
	log_.openFile();
}

void LogCut::operator()()
{
	if (!openFiles()) {
		// openFiles() closed the log handle and only closeFiles() reopens it:
		// without this, a first start (no log file to cut yet) left write()
		// silently dropping everything for the rest of the session.
		log_.openFile();
		return;
	}
	cut();
	closeFiles();
}

bool LogCut::getTimeFromLine(const std::string& line, time_point& tp)
{
	std::tm tm;
	std::istringstream is(line);
	is >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
	if (is.fail())
		return false;
	tp = system_clock::from_time_t(std::mktime(&tm));
	return true;
}

void LogCut::cut()
{
	std::string line;
	auto nowMinusTimeSpan = now_ - std::chrono::hours(24) * Logging::instance().retentionDays();
	// Decided per timestamped line, never latched: logs are chronological (old
	// lines first), so a sticky flag would discard everything after the first
	// expired line - including all current entries. Lines without a parseable
	// timestamp (multi-line entries) follow the preceding timestamped line.
	bool lineTooOld = false;
	while (std::getline(is_, line))	{
		time_point lineTime;
		if (getTimeFromLine(line, lineTime))
			lineTooOld = lineTime < nowMinusTimeSpan;
		if (!lineTooOld)
			os_ << line << std::endl;
	}
}



} // namespace AFS_Core
