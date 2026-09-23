/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of backgroundsync, the background service of AutoFileSync.
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
#include "stdafx.h"
#ifdef _WIN32
    #include <Windows.h>
    #include <Tlhelp32.h>
    #include <direct.h>
    #include <io.h>
	#include <winutil.h>
#else
    #include <sys/stat.h>
    #include <util.h>
#endif
#include <string.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <ctime>
#include <cstring>
#include <winutil.h>

using std::string;
using std::wstring;

namespace util {

#ifdef _WIN32
string wstring_to_string(const wstring& wstr)
{
//    string retval(ws.begin(), ws.end());
//    return retval;
    if (wstr.empty())
    {
        return std::string();
    }
    size_t size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, &wstr[0], static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string ret = std::string(size, 0);
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, &wstr[0], static_cast<int>(wstr.size()), &ret[0], static_cast<int>(size), NULL, NULL);
    size = 0;
    _locale_t lc = _create_locale(LC_ALL, "en_US.UTF-8");
    errno_t err = _wcstombs_s_l(&size, NULL, 0, &wstr[0], _TRUNCATE, lc);
    //ret = std::string(size, 0);
    err = _wcstombs_s_l(&size, &ret[0], size, &wstr[0], _TRUNCATE, lc);
    _free_locale(lc);
    ret.resize(size - 1);
    return ret;

}

wstring string_to_wstring(const string& s)
{
    wstring retval(s.begin(), s.end());
    return retval;
}


string LastErrorText() 
{
    DWORD errCode = GetLastError();
    char *err;
    if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                        NULL,
                        errCode,
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // default language
                        (LPTSTR) &err,
                        0,
                        NULL))
        return "Unbekannter Fehler";
    static char buffer[1024];
    _snprintf_s(buffer, sizeof(buffer), "%s\n", err);
	const string retval=buffer;
    LocalFree(err);
	return retval;
}
#endif

#ifdef _WIN32
bool fileExists(const std::wstring & StrFilename)
{
    return _waccess(StrFilename.c_str(), 0) == 0;
}

bool fileExists(const std::string & StrFilename)
{
	return _access(StrFilename.c_str(), 0) == 0;
}

bool fileinfo(const std::string& fname, string& timestr, DWORD& filesize)
{
	if (!fileExists(fname)) {
		return false;
	}
	_WIN32_FILE_ATTRIBUTE_DATA data;
    auto rc=GetFileAttributesEx(string_to_wstring(fname).c_str(), GetFileExInfoStandard, &data);
    if (rc == 0xFFFFFFFF) {
		return false;
	}
	SYSTEMTIME stUTC, stLocal;
	if (!FileTimeToSystemTime(&data.ftLastWriteTime, &stUTC))
		return false;
	SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
	// Build a string showing the date and time.
	char buffer[80];
    //LPTSTR lpszString=(LPTSTR)&buffer;
	DWORD dwSize=sizeof(buffer);
    /*DWORD dwRet = */sprintf_s((char*const)&buffer, dwSize, "%02d.%02d.%d %02d:%02d",
		stLocal.wDay, stLocal.wMonth, stLocal.wYear, stLocal.wHour, stLocal.wMinute);
	timestr=buffer;
	filesize=data.nFileSizeLow;
	return true;
}

bool fileinfo(const std::wstring& fname, string& timestr, DWORD& filesize)
{
    if (!fileExists(fname)) {
        return false;
    }
    _WIN32_FILE_ATTRIBUTE_DATA data;
    auto rc=GetFileAttributesEx(fname.c_str(), GetFileExInfoStandard, &data);
    if (rc == 0xFFFFFFFF) {
        return false;
    }
    SYSTEMTIME stUTC, stLocal;
    if (!FileTimeToSystemTime(&data.ftLastWriteTime, &stUTC))
        return false;
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    // Build a string showing the date and time.
    char buffer[80];
    //LPTSTR lpszString=(LPTSTR)&buffer;
    DWORD dwSize=sizeof(buffer);
    /*DWORD dwRet = */sprintf_s((char*const)&buffer, dwSize, "%02d.%02d.%d %02d:%02d",
        stLocal.wDay, stLocal.wMonth, stLocal.wYear, stLocal.wHour, stLocal.wMinute);
    timestr=buffer;
    filesize=data.nFileSizeLow;
    return true;
}

bool fileinfo(const std::wstring& fname, SYSTEMTIME& filetime, DWORD& filesize)
{
    if (!fileExists(fname)) {
        return false;
    }
    _WIN32_FILE_ATTRIBUTE_DATA data;
    auto rc=GetFileAttributesEx(fname.c_str(), GetFileExInfoStandard, &data);
    if (rc == 0xFFFFFFFF) {
        return false;
    }
	SYSTEMTIME stUTC;// , stLocal;
    if (!FileTimeToSystemTime(&data.ftLastWriteTime, &stUTC))
        return false;
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &filetime);
//    // Build a string showing the date and time.
//    char buffer[80];
//    //LPTSTR lpszString=(LPTSTR)&buffer;
//    DWORD dwSize=sizeof(buffer);
//    /*DWORD dwRet = */sprintf_s((char*const)&buffer, dwSize, "%02d.%02d.%d %02d:%02d",
//        stLocal.wDay, stLocal.wMonth, stLocal.wYear, stLocal.wHour, stLocal.wMinute);
//    timestr=buffer;
    filesize=data.nFileSizeLow;
    return true;
}
#endif


#ifdef _WIN32
bool filesEqual(const wstring& file1, const wstring& file2)
{
    FILE * fp1;
    FILE * fp2;
    errno_t err = _wfopen_s(&fp1, file1.c_str(), L"rb");
    if (err!=0)
        return false;
    err = _wfopen_s(&fp2, file2.c_str(), L"rb");
    if (err!=0)
        return false;
    if((fp1 == nullptr) || (fp2 == nullptr))
        return false;
    const int N = 10000;
    char buf1[N];
    char buf2[N];
    do {
        size_t r1 = fread(buf1, 1, N, fp1);
        size_t r2 = fread(buf2, 1, N, fp2);
        if (r1 != r2 || memcmp(buf1, buf2, r1)) {
            fclose(fp1);
            fclose(fp2);
            return false;
        }
    } while (!feof(fp1) || !feof(fp2));
    const bool retval=feof(fp1) && feof(fp2);
    fclose(fp1);
    fclose(fp2);
    return retval;
}
#else
bool filesEqual(const string& lFilePath, const string& rFilePath, const uint64_t buffersize)
{
	std::unique_ptr<char> p_lBuffer{ new char[buffersize]() };
	std::unique_ptr<char> p_rBuffer{ new char[buffersize]() };

	std::ifstream lFile(lFilePath.c_str(), std::ios::in | std::ios::binary);
	std::ifstream rFile(rFilePath.c_str(), std::ios::in | std::ios::binary);


	if (!lFile.good() || !rFile.good())
	{
		return false;
	}

	std::streamsize lReadBytesCount = 0;
	std::streamsize rReadBytesCount = 0;

	do {
		lFile.read(p_lBuffer.get(), buffersize);
		rFile.read(p_rBuffer.get(), buffersize);
		lReadBytesCount = lFile.gcount();
		rReadBytesCount = rFile.gcount();

		if (lReadBytesCount != rReadBytesCount || std::memcmp(p_lBuffer.get(), p_rBuffer.get(), lReadBytesCount) != 0)
		{
			return false;
		}
	} while (lFile.good() || rFile.good());

	return true;
}
#endif

string withoutPath(const string& pathname)
{
	string::size_type pos = pathname.find_last_of('\\');
	if (pos==string::npos)
		pos=pathname.find(':');
	string retval=pathname.substr(pos+1);
	if (!retval.empty() && *retval.rbegin()=='\"')
		retval.erase(retval.length()-1);
	return retval;
}

std::string pathOnly(const std::string& pathname)
{
	const string::size_type pos = pathname.find_last_of('\\');
	if (pos==string::npos)
		return "";
	return pathname.substr(0, pos);
}

#ifdef _WIN32
bool make_path(const std::string& path)
{
	string::size_type pos = path.find('\\');
	while (pos!=string::npos) {
		const string newpath=path.substr(0, pos);
		if (newpath.find(':')!=string::npos && !fileExists(newpath))
			if (_mkdir(newpath.c_str())!=0)
				return false;
		const string::size_type lastpos=pos+1;
		pos = path.find('\\', pos+1);
		if (pos==string::npos && lastpos<string::npos) {
			if (!fileExists(path))
				if (_mkdir(path.c_str())!=0)
					return false;
		}

	}
	return true;
}
#else

#endif
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

#ifdef _WIN32
#pragma warning (push)
#pragma warning(disable: 4996) 
#endif
std::string currentDateTime() 
{
    time_t     now = time(nullptr);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
    return buf;
}

void truncateLogfile(const std::string& fpathname, int hours)
{
	std::vector<string> logentries_;
	{
		std::ifstream in(fpathname);
		string line;
		while (std::getline(in, line)) {
			if (line.length()<21)
				continue;
			const string date=line.substr(0, 19);
            std::tm tm = {0, 0, 0, 0, 0, 0, 0, 0, 0};
			std::stringstream ss(date);
			ss >> std::get_time(&tm, "%Y-%m-%d.%H:%M:%S"); // "%d.%m.%Y %H:%M:%S"
			auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
			auto now=std::chrono::system_clock::now();
			const auto span=std::chrono::duration_cast<std::chrono::hours> (now-tp).count();
			if (span < hours)
				logentries_.push_back(line);
		}
	}
	std::ofstream os(fpathname);
	for (string s: logentries_)
		os << s << '\n';
}

#ifdef _WIN32
void handleLastError(const char *msg)
{
    DWORD errCode = GetLastError();
    LPTSTR err;
    if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                        nullptr,
                        errCode,
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // default language
                        reinterpret_cast<LPTSTR>(&err),
                        0,
                        nullptr))
        return;
    std::ostringstream os;
    os << "Fehler: " << msg << "\n - " << wstring_to_string(err);
    LocalFree(err);
    throw std::runtime_error(os.str());
}
#endif
} // namespace util
