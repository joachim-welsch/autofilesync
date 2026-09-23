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
#pragma once
#include <string>
#include <filesystem>
#include <boost/json.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace util {

std::string toupper(std::string);
std::string withTrailingChar(const std::string&, char);

inline
constexpr char const* toStr(bool b)
{
   return b ? "true" : "false";
}

inline
std::string toString(bool b)
{
   return toStr(b);
}

    std::wstring withTrailingChar(const std::wstring& s, wchar_t c);
    bool fileExists(const std::string& StrFilename);
    bool dirExists(const char* name);
    void createDir(const char* name);
    std::string withoutExtension(const std::string& pathname);

    void handleLastError(const char *msg);
    std::string homeDir();
    bool fileinfo(const std::string& fpathname, uint64_t& filesize, std::filesystem::file_time_type& last_write_time);
    std::string filename(const std::string& pathstr);
    //void copyFile(const std::string& source, const std::string& dest);
    boost::property_tree::ptree parseJsonFile( char const* filename);
}
