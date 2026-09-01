#pragma once
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <fstream>
#include "SmtpMailer.h"
#ifdef WIN32

#endif

// Ordered by severity: shouldLog() compares the enumerators, so a new level
// only works where its position says how severe it is. None stays last as the
// threshold that silences everything.
enum class LogLevel { Debug, Info, Warning, Error, FatalError, None };

std::wstring getDefaultLogFilePath();

class Logging {
	static std::unique_ptr<Logging> instance_;
	static std::once_flag initInstanceFlag_;
	mutable std::mutex mutex_;
	LogLevel		level_ = LogLevel::Info;
#ifdef WIN32
std::wstring	filePathName_ = getDefaultLogFilePath();
std::wstring	editorPathName_ = L"C:\\windows\\notepad";
#else
	std::wstring	filePathName_ = getDefaultLogFilePath();
    std::wstring	editorPathName_ = L""; // TODO linux version
#endif
	uint16_t		retentionDays_ = 7;
	// Recipient for the levels that need a person to act (see LogLevel).
	// Off by default: an install must not start sending anywhere unasked.
	bool			notifyEmailActive_ = false;
	std::wstring	notifyEmail_;
	// The account the notification is sent through.
	SmtpAccount		smtp_;
	std::string		lastNotified_;
	std::chrono::steady_clock::time_point lastNotifyTime_{};
	FILE*			fos_ = nullptr;

	static void initSingleton() {
		instance_.reset(new Logging);
	}

    Logging() = default;
	Logging(const Logging&) = delete;
	Logging& operator=(const Logging&) = delete;

	void writeHead(std::ostream& os, LogLevel level);
	void write(LogLevel level, const std::ostringstream& os);
	void writeToFile(const std::ostringstream& os);
	void notifyFatal(const std::string& logLine);
	bool notifyAgain(const std::string& message);
    bool fileOpen() const { return fos_ != nullptr; } // caller must already hold mutex_
public:
	virtual ~Logging();
	static Logging& instance();
	static void shutdown() { instance_.reset(); }
    // static Logging& instance() {
    // 	std::call_once(initInstanceFlag_, &Logging::initSingleton);
    // 	return *instance_;
    // }
	LogLevel getLogLevel();
	bool shouldLog(LogLevel level);
	void closeFile();
	void openFile();
	void eraseFile();
	std::wstring getLogFilePathName();
	std::wstring getEditorPathName();
	std::string logLevelStr(LogLevel);
	LogLevel getFromString(const std::wstring_view& ll);
	void setLogFilePathName(const std::wstring&);
	void setEditorPathName(const std::wstring&);
	void setLogLevel(LogLevel level);
	uint16_t retentionDays() const { std::lock_guard<std::mutex> lock(mutex_); return retentionDays_; }
	void retentionDays(uint16_t days) { std::lock_guard<std::mutex> lock(mutex_); retentionDays_ = days; }
	bool notifyEmailActive() const { std::lock_guard<std::mutex> lock(mutex_); return notifyEmailActive_; }
	void notifyEmailActive(bool active) { std::lock_guard<std::mutex> lock(mutex_); notifyEmailActive_ = active; }
	std::wstring notifyEmail() const { std::lock_guard<std::mutex> lock(mutex_); return notifyEmail_; }
	void notifyEmail(const std::wstring& address) { std::lock_guard<std::mutex> lock(mutex_); notifyEmail_ = address; }
	SmtpAccount smtpAccount() const { std::lock_guard<std::mutex> lock(mutex_); return smtp_; }
	void smtpAccount(const SmtpAccount& account) { std::lock_guard<std::mutex> lock(mutex_); smtp_ = account; }
	void log(LogLevel level, const std::wstring& msg);
	void log(LogLevel level, const std::string& msg);
    void log(LogLevel level, const std::string& msg, bool flag);
    void log(LogLevel level, const std::string& msg, std::chrono::steady_clock::time_point tp);
	void log(LogLevel level, const std::string& msg1, const std::string& msg2);
    void log(LogLevel level, const std::wstring& msg, const std::wstring& filename);
    void log(LogLevel level, const std::wstring& msg1, const std::string& msg2);
    void log(LogLevel level, const std::wstring& msg, const std::wstring& filename, const std::wstring& filename2);
    void log3(LogLevel level, const std::wstring& msg1, const std::wstring& msg2, const std::wstring& msg3);
    void log4(LogLevel level, const std::wstring& msg1, const std::wstring& msg2, const std::wstring& msg3, const std::wstring& msg4);
};

#define LOG_INFO(msg) \
	Logging::instance().log(LogLevel::Info, msg);

#define LOG_INFO2(msg1, msg2) \
	Logging::instance().log(LogLevel::Info, msg1, msg2);

#define LOG_INFO3(msg1, msg2, msg3) \
	Logging::instance().log(LogLevel::Info, msg1, msg2, msg3);

#define LOG_INFO4(msg1, msg2, msg3, msg4) \
    Logging::instance().log4(LogLevel::Info, msg1, msg2, msg3, msg4);

#define LOG_WARNING(msg) \
	Logging::instance().log(LogLevel::Warning, msg);

#define LOG_WARNING2(msg1, msg2) \
    Logging::instance().log(LogLevel::Warning, msg1, msg2);

#define LOG_ERROR(msg) \
	Logging::instance().log(LogLevel::Error, msg);

#define LOG_ERROR2(msg1, msg2) \
	Logging::instance().log(LogLevel::Error, msg1, msg2);

#define LOG_ERROR3(msg1, msg2, msg3) \
    Logging::instance().log3(LogLevel::Error, msg1, msg2, msg3);

#define LOG_ERROR4(msg1, msg2, msg3, msg4) \
    Logging::instance().log4(LogLevel::Error, msg1, msg2, msg3, msg4);

#define LOG_FATAL(msg) \
	Logging::instance().log(LogLevel::FatalError, msg);

#define LOG_FATAL2(msg1, msg2) \
	Logging::instance().log(LogLevel::FatalError, msg1, msg2);

#define LOG_FATAL3(msg1, msg2, msg3) \
	Logging::instance().log3(LogLevel::FatalError, msg1, msg2, msg3);

#ifdef _xDEBUG
// Log debug, even if a different log level is set.
#define LOG_DEBUG(msg) \
    {\
    LogLevel savedLevel = Logging::instance().getLogLevel(); \
	Logging::instance().setLogLevel(LogLevel::Debug); \
	Logging::instance().log(LogLevel::Debug, msg); \
    Logging::instance().setLogLevel(savedLevel);\
    }
#else
#define LOG_DEBUG(msg) \
    Logging::instance().log(LogLevel::Debug, msg);

#define LOG_DEBUG2(msg1, msg2) \
    Logging::instance().log(LogLevel::Debug, msg1, msg2);
#endif


inline
std::unique_ptr<Logging> Logging::instance_;
inline
std::once_flag Logging::initInstanceFlag_;
