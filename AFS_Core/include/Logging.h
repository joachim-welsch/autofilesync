#pragma once
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <fstream>
#include "SmtpMailer.h"

namespace AFS_Core {

enum class LogLevel { Debug, Info, Warning, Error, FatalError, None };

std::wstring get_default_log_file_path();

class Logging {
	using LoggingPtr = std::unique_ptr<Logging>;
	static  LoggingPtr 		instance_;
	static std::once_flag 	init_instance_flag_;
	mutable std::mutex 		mutex_;
	LogLevel				level_ = LogLevel::Info;
#ifdef WIN32
	std::wstring	file_path_name_ = get_default_log_file_path();
	std::wstring	editor_path_name_ = L"C:\\windows\\notepad";
#else
	std::wstring	file_path_name_ = get_default_log_file_path();
    std::wstring	editor_path_name_ = L"/usr/bin/nautilus";
#endif
	uint16_t		retention_days_ = 7;
	// Recipient for the levels that need a person to act (see LogLevel).
	// Off by default: an install must not start sending anywhere unasked.
	bool			notify_email_active_ = false;
	std::wstring	notify_email_;
	// The account the notification is sent through.
	SmtpAccount		smtp_;
	std::string		last_notified_;
	std::chrono::steady_clock::time_point last_notify_time_{};
	FILE*			fos_ = nullptr;

	static void init_singleton()
	{
		instance_.reset(new Logging);
	}

    Logging() = default;
	Logging(const Logging&) = delete;
	Logging& operator=(const Logging&) = delete;

	void write_head(std::ostream& os, LogLevel level);
	void write(LogLevel level, const std::ostringstream& os);
	void write_to_file(const std::ostringstream& os);
	void notify_fatal(const std::string& log_line);
	bool notify_again(const std::string& message);
    bool file_open() const { return fos_ != nullptr; } // caller must already hold mutex_
public:
	virtual ~Logging();
	static Logging& instance();
	static void shutdown() { instance_.reset(); }
    // static Logging& instance() {
    // 	std::call_once(init_instance_flag_, &Logging::init_singleton);
    // 	return *instance_;
    // }
	LogLevel get_log_level();
	bool should_log(LogLevel level);
	void close_file();
	void open_file();
	void erase_file();
	std::wstring get_log_file_path_name();
	std::wstring get_editor_path_name();
	std::string log_level_str(LogLevel);
	LogLevel get_from_string(const std::wstring_view& ll);
	void set_log_file_path_name(const std::wstring&);
	void set_editor_path_name(const std::wstring&);
	void set_log_level(LogLevel level);
	uint16_t retention_days() const { std::lock_guard<std::mutex> lock(mutex_); return retention_days_; }
	void retention_days(uint16_t days) { std::lock_guard<std::mutex> lock(mutex_); retention_days_ = days; }
	bool notify_email_active() const { std::lock_guard<std::mutex> lock(mutex_); return notify_email_active_; }
	void notify_email_active(bool active) { std::lock_guard<std::mutex> lock(mutex_); notify_email_active_ = active; }
	std::wstring notify_email() const { std::lock_guard<std::mutex> lock(mutex_); return notify_email_; }
	void notify_email(const std::wstring& address) { std::lock_guard<std::mutex> lock(mutex_); notify_email_ = address; }
	SmtpAccount smtp_account() const { std::lock_guard<std::mutex> lock(mutex_); return smtp_; }
	void smtp_account(const SmtpAccount& account) { std::lock_guard<std::mutex> lock(mutex_); smtp_ = account; }
	void log(LogLevel level, const std::wstring& msg);
	void log(LogLevel level, const std::string& msg);
    void log(LogLevel level, const std::string& msg, bool flag);
    void log(LogLevel level, const std::string& msg, std::chrono::steady_clock::time_point tp);
	void log(LogLevel level, const std::string& msg1, const std::string& msg2);
    void log(LogLevel level, const std::wstring& msg, const std::wstring& filename);
    void log(LogLevel level, const std::wstring& msg1, const std::string& msg2);
    void log(LogLevel level, const std::wstring& msg1, const std::wstring& msg2, const std::wstring& msg3);
    void log(LogLevel level, const std::wstring& msg1, const std::wstring& msg2, const std::wstring& msg3, const std::wstring& msg4);
};

template<class... Args>
void log_at(LogLevel level, Args&&... args)
{
    Logging::instance().log(level, std::forward<Args>(args)...);
}
template<class... Args> void log_debug(Args&&... args) { log_at(LogLevel::Debug, std::forward<Args>(args)...); }
template<class... Args> void log_info(Args&&... args) { log_at(LogLevel::Info, std::forward<Args>(args)...); }
template<class... Args> void log_warning(Args&&... args) { log_at(LogLevel::Warning, std::forward<Args>(args)...); }
template<class... Args> void log_error(Args&&... args) { log_at(LogLevel::Error, std::forward<Args>(args)...); }
template<class... Args> void log_fatal(Args&&... args) { log_at(LogLevel::FatalError, std::forward<Args>(args)...); }


inline
std::unique_ptr<Logging> Logging::instance_;
inline
std::once_flag Logging::init_instance_flag_;

} // namespace AFS_Core
