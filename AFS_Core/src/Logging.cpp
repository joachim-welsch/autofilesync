#include "pch.h"
#include <Logging.h>
#include <StrUtil.h>
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
// Reads the Win32 environment block instead of the CRT cache so that values
// set at runtime are seen.
std::wstring env_var(const wchar_t* name)
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
	// As on Linux (~/.cache/autofilesync) the log belongs in the user's own data
	// directory, not in %TEMP%: cleanup tools purge that, and all users of the
	// machine would share one file name. Logging::create_dir() creates it on demand.
	std::wstring base = env_var(L"LOCALAPPDATA");
	if (base.empty())
		base = env_var(L"APPDATA");
	if (base.empty()) {
		const std::wstring profile = env_var(L"USERPROFILE");
		if (!profile.empty())
			base = profile + L"\\AppData\\Local";
	}
	if (!base.empty())
		return base + L"\\autofilesync\\autofilesync.log";

	// Last resort when the environment is completely empty (service context).
	wchar_t temp_path[MAX_PATH];
	DWORD len = GetTempPathW(MAX_PATH, temp_path);
	if (len > 0 && len < MAX_PATH) {
		return std::wstring(temp_path) + L"autofilesync.log";
	}
	return L"C:\\Temp\\autofilesync.log";
}
#else
// Logging.h's non-WIN32 default used to be a hardcoded developer path
// (/home/welschj/temp/autofilesync.log) - the equivalent of this function
// simply was never written for Linux/macOS, so it fell back to a literal
// path that exists on exactly one machine. On any other install, logging
// silently produced nothing (open_file() failing is now tolerated, not
// fixed) unless a config file already existed and happened to override it.
std::wstring get_default_log_file_path()
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

// Counts from the last enumerator, so the table below covers every level.
// Leaving None out made log_level_str(LogLevel::None) read past the end.
constexpr int count_enum_elements()
{
	return static_cast<int>(LogLevel::None) + 1;
}

static constexpr std::array<std::string_view, count_enum_elements()> LOG_LEVEL_NAMES = {"Debug", "Info", "Warning", "Error", "FatalError", "None"};

// How long the same fatal error stays silent after it was reported once.
constexpr auto NOTIFY_REPEAT_INTERVAL = std::chrono::minutes(30);

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
	// The error_code overload: a path whose parent conflicts with an existing
	// non-directory file (bad user input, a stale file left where a directory
	// should be) must not throw here - open_file() below already tolerates
	// fopen() failing afterward and must get the chance to.
	std::error_code ec;
	fs::create_directories(log_path.parent_path(), ec);
}

void Logging::open_file()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (fos_ != nullptr)
		return;
	create_dir(file_path_name_);
	fos_=std::fopen(wstring_to_string(file_path_name_).c_str(), "a");
	// Never throw: an invalid/unwritable path (bad user input in LoggingDialog,
	// a directory removed since the last run) must not turn into an uncaught
	// exception. set_log_file_path_name() calls this after already close_file()-ing
	// the previous handle, and LogCut::close_files() calls it unconditionally
	// from MainWindow's constructor on every single startup - either path
	// throwing would crash the whole application instead of just leaving
	// logging disabled (write() already handles fos_ == nullptr gracefully).
}

void Logging::erase_file()
{
	close_file();
	{
		std::lock_guard<std::mutex> lock(mutex_);
		remove(wstring_to_string(file_path_name_).c_str());
	}
	open_file();
}

LogLevel Logging::get_from_string(const wstring_view& ll)
{
	// "None" is not accepted: a configuration that switches the log off
	// entirely would be indistinguishable from a typo, which gets the safe
	// default below - Error keeps the levels that need a person to act.
	for (int i = 0; i < static_cast<int>(LogLevel::None); ++i)
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
	os << timestamp() << " " << Logging::log_level_str(level) << '\t';
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

bool Logging::should_log(LogLevel level)
{
	std::lock_guard<std::mutex> lock(mutex_);
	return static_cast<int>(level) >= static_cast<int>(Logging::level_);
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

void Logging::write_to_file(const std::ostringstream& os)
{
	// Locked for the whole call: close_file()/open_file() (log rotation, called
	// concurrently from GUI/worker threads) must not fclose() fos_ out from
	// under an in-flight write, or this dereferences a dangling/null FILE*.
	std::lock_guard<std::mutex> lock(mutex_);
	if (!file_open())
		return;
	// Never throw from here: LOG_ macros run inside catch handlers and worker
	// threads - an exception escaping them terminates the process exactly when
	// something else (e.g. a full disk) is already going wrong. Failures are
	// silently dropped; there is nowhere sensible left to report them.
	if (fprintf(fos_, "%s", os.str().c_str()) < 1)
		return;
	if (std::fflush(fos_) != 0)
		return;
#ifdef WIN32
	_commit(_fileno(fos_));
#else
	fsync(fileno(fos_));
#endif
}

// Notified outside write_to_file(): that call holds mutex_, which every accessor
// below takes as well, and sending blocks on the network.
void Logging::write(LogLevel level, const std::ostringstream& os)
{
	write_to_file(os);
	if (level == LogLevel::FatalError)
		notify_fatal(os.str());
}

// Strips the timestamped head write_head() put in front, so that the same error
// reported again is recognised as the same error.
static string error_text(const string& log_line)
{
	auto separator = log_line.find('\t');
	return separator == string::npos ? log_line : log_line.substr(separator + 1);
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
	// Sent synchronously although it delays the caller: a fatal error is often
	// followed by the process going down, and a detached thread would be killed
	// with it before the mail is on its way.
	const string error = send_mail(account, recipient, L"autofilesync: schwerwiegender Fehler", log_line);
	if (!error.empty())
		log(LogLevel::Error, "Benachrichtigung an " + wstring_to_string(recipient) + " fehlgeschlagen: ", error);
}

// Identical errors repeat: the daemon retries the same failing sync every cycle
// and would otherwise send one mail per cycle. A different error always passes.
bool Logging::notify_again(const string& message)
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
