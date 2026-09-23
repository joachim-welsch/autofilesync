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
#pragma once

#include <string>
#ifndef _UNIX
    #include <Windows.h>
#endif
#include <chrono>

namespace util {

// Strings

std::string wstring_to_string(const std::wstring& ws);

std::wstring string_to_wstring(const std::string& s);

// Allgemeines
std::string LastErrorText();

// Dateien
bool filesEqual(const std::string& file1, const std::string& file2, const uint64_t buffersize);
bool filesEqual(const std::wstring& file1, const std::wstring& file2);

// Dateisystem
std::string withoutPath(const std::string& pathname);
std::string pathOnly(const std::string& pathname);


bool make_path(const std::string& path);

// Uhrzeit Datum
std::string currentDateTime();

void truncateLogfile(const std::string& fpathname, int hours);

void handleLastError(const char *msg);


//inline
//bool operator>(const SYSTEMTIME& a, const SYSTEMTIME& b)
//{
//    if (a.wYear > b.wYear)
//        return true;
//    else if (a.wYear < b.wYear)
//        return false;
//    else if (a.wMonth > b.wMonth)
//        return true;
//    else if (a.wMonth < b.wMonth)
//        return false;
//    else if (a.wDay > b.wDay)
//        return true;
//    else if (a.wDay < b.wDay)
//        return false;
//    else if (a.wHour > b.wHour)
//        return true;
//    else if (a.wHour < b.wHour)
//        return false;
//    else if (a.wMinute > b.wMinute)
//        return true;
//    else if (a.wMinute < b.wMinute)
//        return false;
//    else if (a.wSecond > b.wSecond)
//        return true;
//    else if (a.wSecond < b.wSecond)
//        return false;
//    return a.wMilliseconds > b.wMilliseconds;

} // namespace util
