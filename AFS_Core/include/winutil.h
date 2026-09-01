#pragma once

#include <string>
#ifdef WIN32
    #include <Windows.h>
#endif
#include <chrono>

namespace util {

// Strings


// Allgemeines
std::wstring LastErrorText();

int windows_version();

// Dateisystem
std::wstring withoutPath(const std::wstring& pathname);

std::wstring pathOnly(const std::wstring& pathname);


// Prozesse
std::wstring start_command(const std::wstring& command);

std::wstring perform(const std::wstring& command);

bool ProcessIsRunning(const std::wstring& name);

bool duplicateProcess(const std::wstring& name);

// Uhrzeit Datum
std::wstring currentDateTime();

void truncateLogfile(const std::wstring& fpathname, int hours);

void handleLastError(const char *msg);

#ifdef WIN32
class AutoHandle {
	HANDLE handle_;
public:
    AutoHandle(): handle_(nullptr) {}
	AutoHandle(HANDLE handle): handle_(handle) {}
	HANDLE& operator=(const HANDLE& other) { handle_=other; return handle_; }
	~AutoHandle() { CloseHandle(handle_); }
	operator HANDLE() { return handle_; }
};

inline
bool operator>(const SYSTEMTIME& a, const SYSTEMTIME& b)
{
    if (a.wYear > b.wYear)
        return true;
    else if (a.wYear < b.wYear)
        return false;
    else if (a.wMonth > b.wMonth)
        return true;
    else if (a.wMonth < b.wMonth)
        return false;
    else if (a.wDay > b.wDay)
        return true;
    else if (a.wDay < b.wDay)
        return false;
    else if (a.wHour > b.wHour)
        return true;
    else if (a.wHour < b.wHour)
        return false;
    else if (a.wMinute > b.wMinute)
        return true;
    else if (a.wMinute < b.wMinute)
        return false;
    else if (a.wSecond > b.wSecond)
        return true;
    else if (a.wSecond < b.wSecond)
        return false;
    return a.wMilliseconds > b.wMilliseconds;
}
#endif

} // namespace util
