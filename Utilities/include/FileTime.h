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
#pragma once
#include <UtilitiesConfig.h>
#include <cstdint>
#include <ctime>
#include <filesystem>

namespace Utilities {

namespace file_time {

    std::time_t to_time_t(const std::filesystem::file_time_type& ft);  // [FileTime.md#conversions]
    std::filesystem::file_time_type to_file_time(std::time_t tt);
    int64_t to_unix_nanoseconds(const std::filesystem::file_time_type& ft);  // [FileTime.md#conversions]
    std::filesystem::file_time_type from_unix_nanoseconds(int64_t ns);
    bool equal(const std::filesystem::file_time_type& a, const std::filesystem::file_time_type& b);
    bool greater(const std::filesystem::file_time_type& a, const std::filesystem::file_time_type& b);
}

} // namespace Utilities
