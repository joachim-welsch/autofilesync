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
#include <chrono>
#include <filesystem>
#include <string>

namespace Utilities {

namespace file_name {

    std::filesystem::path with_timestamp_prefix(const std::filesystem::path& pathname,
        std::chrono::system_clock::time_point time = std::chrono::system_clock::now());  // [FileName.md#with_timestamp_prefix]
    std::wstring sanitized(const std::wstring& name);  // [FileName.md#sanitized]
    std::wstring local_date();  // today as YYYY-MM-DD in local time, for names a person reads
}

} // namespace Utilities
