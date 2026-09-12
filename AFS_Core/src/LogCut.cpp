#include "pch.h"
#include "LogCut.h"
#include <string>
#include <sstream>
#include <ctime>
#include <filesystem>
#include "StrUtil.h"

namespace AFS_Core {

namespace fs = std::filesystem;

using std::chrono::system_clock;

LogCut::LogCut()
    : log_{ Logging::instance() }
    , now_{ system_clock::now() }
{}

static auto native_path(const std::wstring& path)
{
#ifdef _UNIX
    return util::wstring_to_string(path);
#else
	return path;
#endif
}

bool LogCut::open_files()
{
	log_.close_file();
	const std::wstring path_name = log_.get_log_file_path_name();
	is_.open(native_path(path_name), std::ios::in);
	if (!is_.is_open())  // [LogCut.md#temp-file-created-late]
		return false;
	os_.open(native_path(path_name + L".tmp"), std::ios::out);
	return os_.is_open();
}

void LogCut::close_files()
{
	is_.close();
	os_.close();
	std::error_code ec;  // [LogCut.md#error-code-overload]
	fs::rename(log_.get_log_file_path_name() + L".tmp", log_.get_log_file_path_name(), ec);
	log_.open_file();
	if (ec)
		log_error(L"Logdatei konnte nicht gekuerzt werden: ", log_.get_log_file_path_name(), util::string_to_wstring(ec.message()));
}

void LogCut::operator()()
{
	if (!open_files()) {
		log_.open_file();  // [LogCut.md#reopen-log-handle]
		return;
	}
	cut();
	close_files();
}

bool LogCut::get_time_from_line(const std::string& line, TimePoint& tp)
{
	std::tm tm{};
	std::istringstream is(line);
	is >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
	if (is.fail())
		return false;
	tm.tm_isdst = -1; // get_time leaves it unset; mktime then determines daylight saving itself
	tp = system_clock::from_time_t(std::mktime(&tm));
	return true;
}

void LogCut::cut()
{
	std::string line;
	auto now_minus_time_span = now_ - std::chrono::hours(24) * Logging::instance().retention_days();
	bool line_too_old = false;  // [LogCut.md#per-line-age-decision]
	while (std::getline(is_, line))	{
		TimePoint line_time;
		if (get_time_from_line(line, line_time))
			line_too_old = line_time < now_minus_time_span;
		if (!line_too_old)
			os_ << line << '\n';
	}
}

} // namespace AFS_Core
