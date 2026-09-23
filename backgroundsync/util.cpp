/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of backgroundsync, the background service of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
#include "stdafx.h"
#include "util.h"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <sys/stat.h>
#ifndef _WIN32
	#include <unistd.h>
	#include <pwd.h>
#endif
#include <sys/types.h>
#include <filesystem>
#include  <cctype>
#include "logging.h"

using std::string;
using std::wstring;

namespace fs=std::filesystem;
namespace json = boost::json;

namespace util {

std::string toupper(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return std::toupper(c); } // correct
	);
	return s;
}

#ifndef WIN32
std::string homeDir()
{
    const char *homedir;
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
    return homedir;
}
#endif

std::string withTrailingChar(const std::string& s, char c)
{
    string retval=s;
    if (*retval.rbegin()!=c)
        retval+=c;
    return retval;
}

std::wstring withTrailingChar(const std::wstring& s, wchar_t c)
{
    wstring retval=s;
    if (*retval.rbegin()!=c)
        retval+=c;
    return retval;
}

std::string withoutExtension(const std::string& pathname)
{
    string::size_type lppos = pathname.find_last_of('.');
    if (lppos != string::npos)
        return pathname.substr(0, lppos);
    else
        return pathname;
}

bool fileExists(const std::string & StrFilename)
{
    return fs::exists(fs::status(StrFilename));
}

#ifndef  WIN32
bool make_path(const std::string& path)
{
    string::size_type pos = path.find('\\');
    while (pos!=string::npos) {
        const string newpath=path.substr(0, pos);
        if (newpath.find(':')!=string::npos && !fileExists(newpath))
            if (mkdir(newpath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)!=0)
                return false;
        const string::size_type lastpos=pos+1;
        pos = path.find('\\', pos+1);
        if (pos==string::npos && lastpos<string::npos) {
            if (!fileExists(path))
                if (mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)!=0)
                    return false;
        }

    }
    return true;
}
#endif

std::string filename(const std::string& pathstr)
{
	string retval;
	auto pos = pathstr.find_last_of('\\');
	if (pos == string::npos)
		return pathstr;
	retval = pathstr.substr(pos + 1);
	return retval;
}

template <typename TP>
std::time_t to_time_t(TP tp)
{
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now()
              + system_clock::now());
    return system_clock::to_time_t(sctp);
}

std::string toString(const std::wstring& wide)
{
	return string(wide.begin(), wide.end());
}

bool fileinfo(const std::string& fpathname, uint64_t& filesize, fs::file_time_type& last_write_time)
{
    try {
        filesize=fs::file_size(fpathname);
        last_write_time=fs::last_write_time(fpathname);
//        std::time_t tt = to_time_t(last_write_time);
//        std::tm *gmt = std::gmtime(&tt);
//        std::stringstream buffer;
//        buffer << std::put_time(gmt, "%A, %d %B %Y %H:%M");
//        timestr = buffer.str();
        return true;
    }
    catch(std::exception& ex) {
        return false;
    }
}

bool dirExists(const char* name)
{
    struct stat st;
    if (stat(name,&st) == 0)
        if((st.st_mode & S_IFDIR)  == S_IFDIR)
            return true;
    return false;
}

void createDir(const char* name)
{
    mkdir(name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

//bool fileExists(const std::string & name)
//{
//    struct stat buffer;
//    return (stat (name.c_str(), &buffer) == 0);
//}

//void copyFile(const std::string& src, const std::string& destination)
//{
//    int source = open(src.c_str(), O_RDONLY, 0);
//    int dest = open(destination.c_str(), O_WRONLY | O_CREAT /*| O_TRUNC/**/, 0644);

//    // struct required, rationale: function stat() exists also
//    struct stat stat_source;
//    fstat(source, &stat_source);

//    sendfile(dest, source, 0, stat_source.st_size);

//    close(source);
//    close(dest);
//}

//bool fileinfo(const std::string& fpathname, std::string& timestr, uint64_t& filesize)
//{
//    int hfile = open(fpathname.c_str(), O_RDONLY, 0);
//    if (hfile == -1)
//        return false;
//    struct stat stat_file;
//    fstat(hfile, &stat_file);
//    filesize=stat_file.st_size;
//    char datestring[256];
//    struct tm *tm=localtime(&stat_file.st_mtime);
//    strftime(datestring,sizeof(datestring),"%Y-%m-%d-%H.%M.%S" , tm);
//    timestr=datestring;
//    close(hfile);
//    return true;
//}

bool fileinfo(const std::string& fpathname, fs::file_time_type& time, uint64_t& filesize)
{
    try {
        filesize=fs::file_size(fpathname);
        time=fs::last_write_time(fpathname);
        return true;
    }
    catch(std::exception& ex) {
        return false;
    }
}
void handleLastError(const char *msg)
{
    std::cerr << msg << std::endl;
}

boost::property_tree::ptree parseJsonFile(char const* filename )
{
    boost::property_tree::ptree pt;
    try {
        boost::property_tree::read_json(filename, pt);
    } catch (std::exception& ex) {
        TRACE("exeption parseJsonFile: ", ex.what());
    }
    return pt;
}

} // namespace util
