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
#include <FileTime.h>
#include <chrono>

namespace Utilities {

namespace fs = std::filesystem;

namespace file_time {

std::time_t to_time_t(const fs::file_time_type& ft)  // [FileTime.md#conversions]
{
    using namespace std::chrono;
#ifdef _MACOS  // clock_cast is unavailable in Apple's libc++
    const auto sys = file_clock::to_sys(ft);
#else
    const auto sys = clock_cast<system_clock>(ft);
#endif
    return system_clock::to_time_t(time_point_cast<system_clock::duration>(sys));
}

fs::file_time_type to_file_time(std::time_t tt)  // [FileTime.md#conversions]
{
    using namespace std::chrono;
    const auto sys = system_clock::from_time_t(tt);
#ifdef _MACOS
    return time_point_cast<fs::file_time_type::duration>(file_clock::from_sys(sys));
#else
    return time_point_cast<fs::file_time_type::duration>(clock_cast<file_clock>(sys));
#endif
}

int64_t to_unix_nanoseconds(const fs::file_time_type& ft)  // [FileTime.md#conversions]
{
    using namespace std::chrono;
#ifdef _MACOS
    const auto sys = file_clock::to_sys(ft);
#else
    const auto sys = clock_cast<system_clock>(ft);
#endif
    return duration_cast<nanoseconds>(sys.time_since_epoch()).count();
}

fs::file_time_type from_unix_nanoseconds(int64_t ns)  // [FileTime.md#conversions]
{
    using namespace std::chrono;
    const auto sys = time_point_cast<fs::file_time_type::duration>(sys_time<nanoseconds>{nanoseconds(ns)});  // [FileTime.md#overflow]
#ifdef _MACOS
    return file_clock::from_sys(sys);
#else
    return clock_cast<file_clock>(sys);
#endif
}

bool equal(const std::filesystem::file_time_type& a, const std::filesystem::file_time_type& b)
{
#ifdef _MACOS  // clock_cast is unavailable in Apple's libc++, so file_clock::to_sys is used
    return std::chrono::file_clock::to_sys(a) == std::chrono::file_clock::to_sys(b);
#else
    return a == b;
#endif
}

bool greater(const std::filesystem::file_time_type& a, const std::filesystem::file_time_type& b)
{
#ifdef _MACOS  // clock_cast is unavailable in Apple's libc++, so file_clock::to_sys is used
    return std::chrono::file_clock::to_sys(a) > std::chrono::file_clock::to_sys(b);
#else
    return a > b;
#endif
}

} // namespace file_time

} // namespace Utilities
