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
// Liest aus dem Win32-Umgebungsblock statt aus dem CRT-Cache, damit auch zur
// Laufzeit gesetzte Werte gesehen werden.
std::wstring envVar(const wchar_t* name)
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

std::wstring getDefaultLogFilePath()
{
	// Wie unter Linux (~/.cache/autofilesync) gehoert das Log ins benutzereigene
	// Datenverzeichnis, nicht nach %TEMP%: dort raeumen Bereinigungswerkzeuge es
	// weg, und alle Benutzer der Maschine teilen sich denselben Dateinamen.
	// Das Verzeichnis wird von Logging::createDir() bei Bedarf angelegt.
	std::wstring base = envVar(L"LOCALAPPDATA");
	if (base.empty())
		base = envVar(L"APPDATA");
	if (base.empty()) {
		const std::wstring profile = envVar(L"USERPROFILE");
		if (!profile.empty())
			base = profile + L"\\AppData\\Local";
	}
	if (!base.empty())
		return base + L"\\autofilesync\\autofilesync.log";

	// Letzte Reserve, wenn die Umgebung voellig leer ist (Dienstkontext).
	wchar_t tempPath[MAX_PATH];
	DWORD len = GetTempPathW(MAX_PATH, tempPath);
	if (len > 0 && len < MAX_PATH) {
		return std::wstring(tempPath) + L"autofilesync.log";
	}
	return L"C:\\Temp\\autofilesync.log";
}
#else
// Logging.h's non-WIN32 default used to be a hardcoded developer path
// (/home/welschj/temp/autofilesync.log) - the equivalent of this function
// simply was never written for Linux/macOS, so it fell back to a literal
// path that exists on exactly one machine. On any other install, logging
// silently produced nothing (openFile() failing is now tolerated, not
// fixed) unless a config file already existed and happened to override it.
std::wstring getDefaultLogFilePath()
{
	const char* home = getenv("HOME");
	if (!home)
		return L"/tmp/autofilesync.log";
	const char* xdgCache = getenv("XDG_CACHE_HOME");
	std::string base = (xdgCache && *xdgCache) ? xdgCache : std::string(home) + "/.cache";
	return string_to_wstring(base + "/autofilesync/autofilesync.log");
}
#endif

Logging& Logging::instance() {
	std::call_once(initInstanceFlag_, &Logging::initSingleton);
	if (!instance_) {
		instance_.reset(new Logging);
	}
	return *instance_;
}

// Counts from the last enumerator, so the table below covers every level.
// Leaving None out made logLevelStr(LogLevel::None) read past the end.
constexpr int countEnumElements() {
	return static_cast<int>(LogLevel::None) + 1;
}

static std::array<std::string_view, countEnumElements()> LogLevelStr = {"Debug", "Info", "Warning", "Error", "FatalError", "None"};

// How long the same fatal error stays silent after it was reported once.
constexpr auto notifyRepeatInterval = std::chrono::minutes(30);

Logging::~Logging()
{
	closeFile();
}

void Logging::closeFile()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (fos_ != nullptr)
		std::fclose(fos_);
	fos_ = nullptr;
}

void createDir(const wstring& filePathName)
{
	fs::path logPath(filePathName);
	// The error_code overload: a path whose parent conflicts with an existing
	// non-directory file (bad user input, a stale file left where a directory
	// should be) must not throw here - openFile() below already tolerates
	// fopen() failing afterward and must get the chance to.
	std::error_code ec;
	fs::create_directories(logPath.parent_path(), ec);
}

void Logging::openFile()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (fos_ != nullptr)
		return;
	createDir(filePathName_);
	fos_=std::fopen(wstring_to_string(filePathName_).c_str(), "a");
	// Never throw: an invalid/unwritable path (bad user input in LoggingDialog,
	// a directory removed since the last run) must not turn into an uncaught
	// exception. setLogFilePathName() calls this after already closeFile()-ing
	// the previous handle, and LogCut::closeFiles() calls it unconditionally
	// from MainWindow's constructor on every single startup - either path
	// throwing would crash the whole application instead of just leaving
	// logging disabled (write() already handles fos_ == nullptr gracefully).
}

void Logging::eraseFile()
{
	closeFile();
	{
		std::lock_guard<std::mutex> lock(mutex_);
		remove(wstring_to_string(filePathName_).c_str());
	}
	openFile();
}

LogLevel Logging::getFromString(const wstring_view& ll)
{
	if (ll == L"Debug") return LogLevel::Debug;
		else if (ll == L"Info") return LogLevel::Info;
		else if (ll == L"Warning") return LogLevel::Warning;
		else if (ll == L"Error") return LogLevel::Error;
		else if (ll == L"FatalError") return LogLevel::FatalError;
		// Deliberately not the highest level: an unreadable entry in the
		// configuration is a typo, and answering it with FatalError would
		// silently turn the log down to almost nothing. Error keeps the
		// levels that need a person to act, which is the safe default here.
		else return	LogLevel::Error;
}

std::string timestamp()
{
	using namespace std::chrono;
	using clock = system_clock;

	const auto current_time_point{ clock::now() };
	const auto current_time{ clock::to_time_t(current_time_point) };
	#ifdef WIN32
		std::tm current_localtime;
		localtime_s(&current_localtime, &current_time);
	#else
		const auto  current_localtime { *std::localtime(&current_time) };
	#endif
	const auto current_time_since_epoch{ current_time_point.time_since_epoch() };
	const auto current_milliseconds{ duration_cast<milliseconds> (current_time_since_epoch).count() % 1000 };

	std::ostringstream stream;
	stream << std::put_time(&current_localtime, "%Y-%m-%d %H:%M:%S") << "." << std::setw(3) << std::setfill('0') << current_milliseconds;
	return stream.str();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//Logging::Logging() 
//	: os_(wstring_to_string(filePathName_), std::ios::app)
//{
//}

void Logging::writeHead(std::ostream& os, LogLevel level)
{
	os << timestamp() << " " << Logging::logLevelStr(level) << '\t';
}

std::wstring Logging::getLogFilePathName()
{
	std::lock_guard<std::mutex> lock(mutex_);
	return filePathName_;
}

std::wstring Logging::getEditorPathName()
{
	std::lock_guard<std::mutex> lock(mutex_);
	return editorPathName_;
}

std::string Logging::logLevelStr(LogLevel l)
{
	return LogLevelStr[static_cast<int>(l)].data();
}

bool Logging::shouldLog(LogLevel level)
{
	std::lock_guard<std::mutex> lock(mutex_);
	return static_cast<int>(level) >= static_cast<int>(Logging::level_);
}

LogLevel Logging::getLogLevel()
{
	std::lock_guard<std::mutex> lock(mutex_);
	return level_;
}

void Logging::setLogLevel(LogLevel plevel)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (level_ == plevel)
		return;
	level_ = plevel;
}

void Logging::setLogFilePathName(const std::wstring& newLogPathname)
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (filePathName_ == newLogPathname)
			return;
	}
	closeFile();
	{
		std::lock_guard<std::mutex> lock(mutex_);
		filePathName_ = newLogPathname;
	}
	openFile();
}

void Logging::setEditorPathName(const std::wstring& newEditorPathname)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (editorPathName_ == newEditorPathname)
		return;
	editorPathName_ = newEditorPathname;
}

void Logging::log(LogLevel level, const std::wstring& msg)
{	
	log(level, wstring_to_string(msg));
}

void Logging::writeToFile(const std::ostringstream& os)
{
	// Locked for the whole call: closeFile()/openFile() (log rotation, called
	// concurrently from GUI/worker threads) must not fclose() fos_ out from
	// under an in-flight write, or this dereferences a dangling/null FILE*.
	std::lock_guard<std::mutex> lock(mutex_);
	if (!fileOpen())
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

// Notified outside writeToFile(): that call holds mutex_, which every accessor
// below takes as well, and sending blocks on the network.
void Logging::write(LogLevel level, const std::ostringstream& os)
{
	writeToFile(os);
	if (level == LogLevel::FatalError)
		notifyFatal(os.str());
}

// Strips the timestamped head writeHead() put in front, so that the same error
// reported again is recognised as the same error.
static string errorText(const string& logLine)
{
	auto separator = logLine.find('\t');
	return separator == string::npos ? logLine : logLine.substr(separator + 1);
}

void Logging::notifyFatal(const string& logLine)
{
	SmtpAccount account = smtpAccount();
	if (!account.complete())
		return;
	const wstring recipient = notifyEmail();
	if (!notifyEmailActive() || recipient.empty())
		return;
	if (!notifyAgain(errorText(logLine)))
		return;
	// Sent synchronously although it delays the caller: a fatal error is often
	// followed by the process going down, and a detached thread would be killed
	// with it before the mail is on its way.
	const string error = sendMail(account, recipient, L"autofilesync: schwerwiegender Fehler", logLine);
	if (!error.empty())
		log(LogLevel::Error, "Benachrichtigung an " + wstring_to_string(recipient) + " fehlgeschlagen: ", error);
}

// Identical errors repeat: the daemon retries the same failing sync every cycle
// and would otherwise send one mail per cycle. A different error always passes.
bool Logging::notifyAgain(const string& message)
{
	std::lock_guard<std::mutex> lock(mutex_);
	const auto now = std::chrono::steady_clock::now();
	if (message == lastNotified_ && now - lastNotifyTime_ < notifyRepeatInterval)
		return false;
	lastNotified_ = message;
	lastNotifyTime_ = now;
	return true;
}

void Logging::log(LogLevel level, const std::string& msg)
{
	if (shouldLog(level)) {
		std::ostringstream os;
		writeHead(os, level);
		os << msg << std::endl;
		write(level, os);
	}
}

void Logging::log(LogLevel level, const std::string& msg, bool flag)
{
    if (shouldLog(level)) {
        std::ostringstream os;
        writeHead(os, level);
        os << msg << std::boolalpha << flag << std::endl;
        write(level, os);
    }
}

void Logging::log(LogLevel level, const std::string& msg, std::chrono::steady_clock::time_point tp)
{
    if (shouldLog(level)) {
        std::ostringstream os;
        writeHead(os, level);
        using namespace std::chrono;
		auto since_start = duration_cast<seconds>(tp.time_since_epoch()).count();
        os << msg << since_start << std::endl;
        write(level, os);
    }
}

void Logging::log(LogLevel level, const std::string& msg1, const std::string& msg2)
{
	if (shouldLog(level)) {
		std::ostringstream os;
		writeHead(os, level);
		os << msg1 << msg2 << std::endl;
		write(level, os);
	}
}

void Logging::log(LogLevel level, const std::wstring& msg, const std::wstring& filename)
{
	if (shouldLog(level)) {
		std::ostringstream os;
		writeHead(os, level);
        os << wstring_to_string(msg) << " " << wstring_to_string(filename) << std::endl;
		write(level, os);
	}
}

void Logging::log(LogLevel level, const std::wstring& msg, const std::string& filename)
{
    if (shouldLog(level)) {
		std::ostringstream os;
        writeHead(os, level);
        os << wstring_to_string(msg) << " " << filename << std::endl;
		write(level, os);
    }
}

void Logging::log(LogLevel level, const std::wstring& msg, const std::wstring& filename, const std::wstring& filename2)
{
	if (shouldLog(level)) {
		std::ostringstream os;
		writeHead(os, level);
		os << wstring_to_string(msg) << " Dateien: " << wstring_to_string(filename) << ", " << wstring_to_string(filename2) << std::endl;
		write(level, os);
	}
}

void Logging::log3(LogLevel level, const std::wstring& msg1, const std::wstring& msg2, const std::wstring& msg3)
{
    if (shouldLog(level)) {
		std::ostringstream os;
        writeHead(os, level);
        os << wstring_to_string(msg1) << " " << wstring_to_string(msg2) << " " << wstring_to_string(msg3) << std::endl;
		write(level, os);
    }
}

void Logging::log4(LogLevel level, const std::wstring& msg1, const std::wstring& msg2, const std::wstring& msg3, const std::wstring& msg4)
{
    if (shouldLog(level)) {
		std::ostringstream os;
        writeHead(os, level);
        os << wstring_to_string(msg1) << " " << wstring_to_string(msg2) << " " << wstring_to_string(msg3) << " " << wstring_to_string(msg4) << std::endl;
		write(level, os);
    }
}

} // namespace AFS_Core
