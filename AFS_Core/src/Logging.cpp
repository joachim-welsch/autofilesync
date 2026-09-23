/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include <Logging.h>
#include <StrUtil.h>
#include <Translation.h>
#include <array>
#include <chrono>
#include <ctime>
#include <cwctype>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <vector>
#ifdef WIN32
    #include <io.h>
	#include <Windows.h>
#else
    #include <unistd.h>
    #include <sys/stat.h>
#endif

namespace AFS_Core {

namespace fs = std::filesystem;
using util::wstring_to_string;
using util::string_to_wstring;
using std::string;
using std::wstring;
using std::wstring_view;

#ifdef WIN32
namespace {
std::wstring env_var(const wchar_t* name)  // [Logging.md#env_var]
{
	DWORD len = GetEnvironmentVariableW(name, nullptr, 0);
	if (len == 0)
		return {};
	std::wstring value(len, L'\0');
	const DWORD written = GetEnvironmentVariableW(name, &value[0], len);
	value.resize(written);
	return value;
}
}

std::wstring get_default_log_file_path()
{
	std::wstring base = env_var(L"LOCALAPPDATA");  // [Logging.md#windows-log-location]
	if (base.empty())
		base = env_var(L"APPDATA");
	if (base.empty()) {
		const std::wstring profile = env_var(L"USERPROFILE");
		if (!profile.empty())
			base = profile + L"\\AppData\\Local";
	}
	if (!base.empty())
		return base + L"\\autofilesync\\autofilesync.log";

	wchar_t temp_path[MAX_PATH];  // last resort when the environment is completely empty (service context)
	DWORD len = GetTempPathW(MAX_PATH, temp_path);
	if (len > 0 && len < MAX_PATH) {
		return std::wstring(temp_path) + L"autofilesync.log";
	}
	return L"C:\\Temp\\autofilesync.log";
}
#else
std::wstring get_default_log_file_path()  // [Logging.md#get_default_log_file_path]
{
	const char* home = getenv("HOME");
	if (!home)
		return L"/tmp/autofilesync.log";
	const char* xdg_cache = getenv("XDG_CACHE_HOME");
	std::string base = (xdg_cache && *xdg_cache) ? xdg_cache : std::string(home) + "/.cache";
	return string_to_wstring(base + "/autofilesync/autofilesync.log");
}
#endif

Logging& Logging::instance()
{
	std::call_once(init_instance_flag_, &Logging::init_singleton);
	if (!instance_) {
		instance_.reset(new Logging);
	}
	return *instance_;
}

constexpr int count_enum_elements()  // [Logging.md#count_enum_elements]
{
	return static_cast<int>(LogLevel::None) + 1;
}

static constexpr std::array<std::string_view, count_enum_elements()> LOG_LEVEL_NAMES = {"Debug", "Info", "Warning", "Error", "FatalError", "None"};

constexpr auto LOG_REPEAT_INTERVAL = std::chrono::minutes(30);  // [Logging.md#LOG_REPEAT_INTERVAL]
constexpr size_t MAX_REPETITIONS = 256;  // [Logging.md#MAX_REPETITIONS]
constexpr size_t MAX_COLLECTED = 200;  // [Logging.md#collected_]
constexpr auto NOTIFY_REPEAT_INTERVAL = std::chrono::minutes(30);  // [Logging.md#NOTIFY_REPEAT_INTERVAL]

Logging::~Logging()
{
	close_file();
}

void Logging::close_file()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (fos_ != nullptr)
		std::fclose(fos_);
	fos_ = nullptr;
}

void create_dir(const wstring& file_path_name)
{
	fs::path log_path(file_path_name);
	std::error_code ec;  // [Logging.md#create_dir-never-throws]
	fs::create_directories(log_path.parent_path(), ec);
}

void Logging::open_file()
{
	std::lock_guard<std::mutex> lock(mutex_);
	open_file_locked();
}

void Logging::open_file_locked()
{
	if (fos_ != nullptr)
		return;
	create_dir(file_path_name_);
	fos_=std::fopen(wstring_to_string(file_path_name_).c_str(), "a");  // [Logging.md#open_file-never-throws]
}

bool Logging::file_replaced() const  // [Logging.md#follow_replaced_file]
{
#ifdef WIN32
	return false;
#else
	struct stat open_file_stat{}, path_stat{};
	if (fstat(fileno(fos_), &open_file_stat) != 0)
		return false;
	if (stat(wstring_to_string(file_path_name_).c_str(), &path_stat) != 0)
		return true;
	return open_file_stat.st_ino != path_stat.st_ino || open_file_stat.st_dev != path_stat.st_dev;
#endif
}

void Logging::follow_replaced_file()
{
	if (!file_open() || !file_replaced())
		return;
	std::fclose(fos_);
	fos_ = nullptr;
	open_file_locked();
}

void Logging::erase_file()
{
	close_file();
	{
		std::lock_guard<std::mutex> lock(mutex_);
		remove(wstring_to_string(file_path_name_).c_str());
		repetitions_.clear();  // [Logging.md#repetitions-follow-the-file]
	}
	open_file();
}

LogLevel Logging::get_from_string(const wstring_view& ll)
{
	for (int i = 0; i < static_cast<int>(LogLevel::None); ++i)  // [Logging.md#log-level-from-string]
		if (ll == string_to_wstring(std::string(LOG_LEVEL_NAMES[i])))
			return static_cast<LogLevel>(i);
	return LogLevel::Error;
}

std::string timestamp()
{
	using namespace std::chrono;
	using Clock = system_clock;

	const auto current_time_point{ Clock::now() };
	const auto current_time{ Clock::to_time_t(current_time_point) };
	std::tm current_localtime{};
	#ifdef WIN32
		localtime_s(&current_localtime, &current_time);
	#else
		localtime_r(&current_time, &current_localtime);
	#endif
	const auto current_time_since_epoch{ current_time_point.time_since_epoch() };
	const auto current_milliseconds{ duration_cast<milliseconds> (current_time_since_epoch).count() % 1000 };

	std::ostringstream stream;
	stream << std::put_time(&current_localtime, "%Y-%m-%d %H:%M:%S") << "." << std::setw(3) << std::setfill('0') << current_milliseconds;
	return stream.str();
}

void Logging::write_head(std::ostream& os, LogLevel level)
{
	os << timestamp() << " " << static_cast<char>(origin()) << " " << Logging::log_level_str(level) << '\t';
}

std::wstring Logging::get_log_file_path_name()
{
	std::lock_guard<std::mutex> lock(mutex_);
	return file_path_name_;
}

std::wstring Logging::get_editor_path_name()
{
	std::lock_guard<std::mutex> lock(mutex_);
	return editor_path_name_;
}

std::string Logging::log_level_str(LogLevel l)
{
	return LOG_LEVEL_NAMES[static_cast<int>(l)].data();
}

static string error_text(const string& log_line);

bool Logging::should_log(LogLevel level)
{
	std::lock_guard<std::mutex> lock(mutex_);
	return level >= level_ || (collecting_ && level >= LogLevel::Info);  // [Logging.md#collected_]
}

void Logging::collect_messages(bool on)
{
	std::lock_guard<std::mutex> lock(mutex_);
	collecting_ = on;
	if (on)
		collected_.clear();
}

std::vector<LogMessage> Logging::take_collected_messages()
{
	std::lock_guard<std::mutex> lock(mutex_);
	return std::exchange(collected_, {});
}

void Logging::collect(LogLevel level, const string& line)  // [Logging.md#collected_]
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!collecting_ || level < LogLevel::Info || collected_.size() >= MAX_COLLECTED)
		return;
	string text = error_text(line);
	if (!text.empty() && text.back() == '\n')
		text.pop_back();
	collected_.push_back({level, text});
}

LogLevel Logging::get_log_level()
{
	std::lock_guard<std::mutex> lock(mutex_);
	return level_;
}

void Logging::set_log_level(LogLevel plevel)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (level_ == plevel)
		return;
	level_ = plevel;
}

void Logging::set_log_file_path_name(const std::wstring& new_log_pathname)
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (file_path_name_ == new_log_pathname)
			return;
	}
	close_file();
	{
		std::lock_guard<std::mutex> lock(mutex_);
		file_path_name_ = new_log_pathname;
		repetitions_.clear();  // [Logging.md#repetitions-follow-the-file]
	}
	open_file();
}

void Logging::set_editor_path_name(const std::wstring& new_editor_pathname)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (editor_path_name_ == new_editor_pathname)
		return;
	editor_path_name_ = new_editor_pathname;
}

void Logging::log(LogLevel level, const std::wstring& msg)
{	
	log(level, wstring_to_string(msg));
}

static string error_text(const string& log_line)  // [Logging.md#error_text]
{
	auto separator = log_line.find('\t');
	return separator == string::npos ? log_line : log_line.substr(separator + 1);
}

void Logging::forget_expired_repetitions(std::chrono::steady_clock::time_point now)
{
	if (repetitions_.size() < MAX_REPETITIONS)
		return;
	std::erase_if(repetitions_, [now](const auto& entry) { return now - entry.second.written >= LOG_REPEAT_INTERVAL; });
	if (repetitions_.size() >= MAX_REPETITIONS)
		repetitions_.clear();
}

bool Logging::report_again(string& line)  // [Logging.md#report_again]
{
	const auto now = std::chrono::steady_clock::now();
	forget_expired_repetitions(now);
	const auto [entry, first_time] = repetitions_.try_emplace(error_text(line), Repetition{now});
	if (first_time)
		return true;
	Repetition& repetition = entry->second;
	if (now - repetition.written < LOG_REPEAT_INTERVAL) {
		++repetition.suppressed;
		return false;
	}
	if (repetition.suppressed > 0 && !line.empty())
		line.insert(line.size() - 1, " " + translation::translate_utf8("log_message_repeated", {std::to_string(repetition.suppressed)}));  // in front of the line break
	repetition = Repetition{now};
	return true;
}

void Logging::write_to_file(LogLevel level, string line)
{
	std::lock_guard<std::mutex> lock(mutex_);  // [Logging.md#write_to_file-locking]
	if (level < level_)  // [Logging.md#collected_]
		return;
	if (level >= LogLevel::Warning && !report_again(line))
		return;
	follow_replaced_file();
	if (!file_open())
		return;
	if (fprintf(fos_, "%s", line.c_str()) < 1)  // [Logging.md#write-failures-dropped]
		return;
	if (std::fflush(fos_) != 0)
		return;
#ifdef WIN32
	_commit(_fileno(fos_));
#else
	fsync(fileno(fos_));
#endif
}

void Logging::write(LogLevel level, const std::ostringstream& os)  // [Logging.md#write-notifies-outside-lock]
{
	collect(level, os.str());
	write_to_file(level, os.str());
	if (level == LogLevel::FatalError)
		notify_fatal(os.str());
}

void Logging::notify_fatal(const string& log_line)
{
	SmtpAccount account = smtp_account();
	if (!account.complete())
		return;
	const wstring recipient = notify_email();
	if (!notify_email_active() || recipient.empty())
		return;
	if (!notify_again(error_text(log_line)))
		return;
	const string error = send_mail(account, recipient, translation::translate("fatal_error_mail_subject"), log_line);  // [Logging.md#mail-sent-synchronously]
	if (!error.empty())
		log(LogLevel::Error, translation::translate_utf8("notification_failed",
			{wstring_to_string(recipient)}), error);
}

bool Logging::notify_again(const string& message)  // [Logging.md#notify_again]
{
	std::lock_guard<std::mutex> lock(mutex_);
	const auto now = std::chrono::steady_clock::now();
	if (message == last_notified_ && now - last_notify_time_ < NOTIFY_REPEAT_INTERVAL)
		return false;
	last_notified_ = message;
	last_notify_time_ = now;
	return true;
}

void Logging::log(LogLevel level, const std::string& msg)
{
	if (should_log(level)) {
		std::ostringstream os;
		write_head(os, level);
		os << msg << std::endl;
		write(level, os);
	}
}

void Logging::log(LogLevel level, const std::string& msg, bool flag)
{
    if (should_log(level)) {
        std::ostringstream os;
        write_head(os, level);
        os << msg << std::boolalpha << flag << std::endl;
        write(level, os);
    }
}

void Logging::log(LogLevel level, const std::string& msg, std::chrono::steady_clock::time_point tp)
{
    if (should_log(level)) {
        std::ostringstream os;
        write_head(os, level);
        using namespace std::chrono;
		auto since_start = duration_cast<seconds>(tp.time_since_epoch()).count();
        os << msg << since_start << std::endl;
        write(level, os);
    }
}

void Logging::log(LogLevel level, const std::string& msg1, const std::string& msg2)
{
	if (should_log(level)) {
		std::ostringstream os;
		write_head(os, level);
		os << msg1 << msg2 << std::endl;
		write(level, os);
	}
}

void Logging::log(LogLevel level, const std::wstring& msg, const std::wstring& filename)
{
	if (should_log(level)) {
		std::ostringstream os;
		write_head(os, level);
        os << wstring_to_string(msg) << " " << wstring_to_string(filename) << std::endl;
		write(level, os);
	}
}

void Logging::log(LogLevel level, const std::wstring& msg, const std::string& filename)
{
    if (should_log(level)) {
		std::ostringstream os;
        write_head(os, level);
        os << wstring_to_string(msg) << " " << filename << std::endl;
		write(level, os);
    }
}

void Logging::log(LogLevel level, const std::wstring& msg1, const std::wstring& msg2, const std::wstring& msg3)
{
    if (should_log(level)) {
		std::ostringstream os;
        write_head(os, level);
        os << wstring_to_string(msg1) << " " << wstring_to_string(msg2) << " " << wstring_to_string(msg3) << std::endl;
		write(level, os);
    }
}

void Logging::log(LogLevel level, const std::wstring& msg1, const std::wstring& msg2, const std::wstring& msg3, const std::wstring& msg4)
{
    if (should_log(level)) {
		std::ostringstream os;
        write_head(os, level);
        os << wstring_to_string(msg1) << " " << wstring_to_string(msg2) << " " << wstring_to_string(msg3) << " " << wstring_to_string(msg4) << std::endl;
		write(level, os);
    }
}

} // namespace AFS_Core
