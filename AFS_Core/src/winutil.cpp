#include "pch.h"
#include "winutil.h"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#ifdef WIN32
    #include <Tlhelp32.h>
    #include <Windows.h>
    #include <direct.h>
    #include <io.h>
#endif
#include "util.h"
#include <codecvt>

using std::wstring;
using std::string;

namespace util {

std::wstring pathOnly(const std::wstring& pathname)
{
	const wstring::size_type pos = pathname.find_last_of('\\');
	if (pos==wstring::npos)
		return L"";
	return pathname.substr(0, pos);
}

/* veraltet
int windows_version()
{
	OSVERSIONINFO osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	return osvi.dwMajorVersion;
}
*/
int windows_version()
{
	return 7;
}

#pragma warning (push)
#pragma warning(disable: 4996) 
std::wstring currentDateTime() 
{
    time_t     now = time(nullptr);
    struct tm  tstruct;
    wchar_t       buf[80];
    tstruct = *localtime(&now);
    wcsftime(buf, sizeof(buf) / 2, L"%Y-%m-%d.%X", &tstruct);
    return buf;
}
#pragma warning (pop)


} // namespace util
