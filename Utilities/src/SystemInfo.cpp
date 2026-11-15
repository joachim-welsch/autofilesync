/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of Utilities, the general-purpose helpers of AutoFileSync.
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
#include "pch.h"
#include <SystemInfo.h>
#ifdef _WIN32
#  include <process.h>   // _getpid
#  include <windows.h>   // GetComputerNameA
#else
#  include <unistd.h>    // getpid, gethostname
#endif

namespace Utilities {

using std::string;

namespace system_info {

unsigned long current_pid()
{
#ifdef _WIN32
    return static_cast<unsigned long>(::_getpid());
#else
    return static_cast<unsigned long>(::getpid());
#endif
}

string host_name()  // [SystemInfo.md#host_name]
{
    char name[256] = {};
#ifdef _WIN32
    DWORD size = sizeof name;
    if (!GetComputerNameA(name, &size))
        return "localhost";
#else
    if (gethostname(name, sizeof name) != 0)
        return "localhost";
    name[sizeof name - 1] = '\0';
#endif
    return name[0] == '\0' ? "localhost" : name;
}

} // namespace system_info

} // namespace Utilities
