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

static auto native_path(const std::wstring& path)
{
#ifdef _UNIX
    return wstring_to_string(path);
#else
	return path;
#endif
}

bool LogCut::open_files()
{
	log_.close_file();
	const std::wstring path_name = log_.get_log_file_path_name();
	is_.open(native_path(path_name), std::ios::in);
	// The temp file is created only once the log itself opened: on the early
	// return the caller skips close_files(), and with it the rename that turns
	// the temp file back into the log - creating it eagerly left an orphaned
	// ".tmp" behind on every start without an existing log file.
	if (!is_.is_open())
		return false;
	os_.open(native_path(path_name + L".tmp"), std::ios::out);
	return os_.is_open();
}

void LogCut::close_files()
{
	is_.close();
	os_.close();
	fs::rename(log_.get_log_file_path_name() + L".tmp", log_.get_log_file_path_name()); //
	log_.open_file();
}

void LogCut::operator()()
{
	if (!open_files()) {
		// open_files() closed the log handle and only close_files() reopens it:
		// without this, a first start (no log file to cut yet) left write()
		// silently dropping everything for the rest of the session.
		log_.open_file();
		return;
	}
	cut();
	close_files();
}

bool LogCut::get_time_from_line(const std::string& line, TimePoint& tp)
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
	auto now_minus_time_span = now_ - std::chrono::hours(24) * Logging::instance().retention_days();
	// Decided per timestamped line, never latched: logs are chronological (old
	// lines first), so a sticky flag would discard everything after the first
	// expired line - including all current entries. Lines without a parseable
	// timestamp (multi-line entries) follow the preceding timestamped line.
	bool line_too_old = false;
	while (std::getline(is_, line))	{
		TimePoint line_time;
		if (get_time_from_line(line, line_time))
			line_too_old = line_time < now_minus_time_span;
		if (!line_too_old)
			os_ << line << std::endl;
	}
}



} // namespace AFS_Core
