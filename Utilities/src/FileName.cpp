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
#include <FileName.h>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Utilities {

using std::wstring;

namespace fs = std::filesystem;

namespace file_name {

static std::tm local_time(std::time_t tt)
{
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    return tm;
}

static std::wstring timestamp(std::chrono::system_clock::time_point time)  // [FileName.md#timestamp]
{
    using namespace std::chrono;
    const auto second = floor<seconds>(time);
    const auto millis = duration_cast<milliseconds>(time - second).count();
    const std::tm tm = local_time(system_clock::to_time_t(second));
    std::wostringstream os;
    os << std::put_time(&tm, L"%Y-%m-%d_%H-%M-%S") << L'-'
       << std::setw(3) << std::setfill(L'0') << millis;
    return os.str();
}

fs::path with_timestamp_prefix(const fs::path& pathname, std::chrono::system_clock::time_point time)
{
    if (pathname.filename().empty())  // [FileName.md#filename-required]
        return pathname;
    return pathname.parent_path() / (timestamp(time) + L"_" + pathname.filename().wstring());
}

wstring local_date()
{
    const std::tm tm = local_time(std::time(nullptr));
    std::wstringstream out;
    out << std::put_time(&tm, L"%Y-%m-%d");
    return out.str();
}

wstring sanitized(const wstring& name)  // [FileName.md#sanitized]
{
    const wstring forbidden = L"<>:\"/\\|?*";
    wstring retval;
    for (wchar_t c : name)
        retval += (c < 32 || forbidden.find(c) != wstring::npos) ? L'_' : c;
    while (!retval.empty() && (retval.back() == L'.' || retval.back() == L' '))
        retval.pop_back();
    return retval;
}

} // namespace file_name

} // namespace Utilities
