/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
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
#include <mutex>
#include <map>
#include <chrono>

namespace AFS_Core {

class CheckPath {
    mutable std::mutex  drive_check_cache_mutex_;

    struct DriveCheckCacheEntry {
        bool result;
        uint16_t time_interval;
        std::chrono::steady_clock::time_point timestamp;
    };
    mutable std::map<std::wstring, DriveCheckCacheEntry> drive_check_cache_;
    static bool expired(const DriveCheckCacheEntry& entry);
    bool both_sides_reachable(const std::wstring& source, const std::wstring& dest) const;
    bool path_exists(const std::wstring& pathname, bool must_exist) const;  // [CheckPath.md#path_exists]
    bool side_reachable(const std::wstring& pathname, bool must_exist) const;  // [CheckPath.md#side_reachable]
public:
    CheckPath() = default;
    bool operator()(const std::wstring& source, const std::wstring& dest, bool ssh);
    bool ssh_path_reachable(const std::wstring& pathname, bool must_exist) const;  // [CheckPath.md#ssh_path_reachable]
};

uint16_t next_check_interval(uint16_t previous_interval, bool repeats_previous_result, bool reachable);  // [CheckPath.md#next_check_interval]

bool path_contains_volume_name(const std::wstring& path);
std::wstring volume_name(const std::wstring& path);
std::wstring root_name(const std::wstring& path);
std::wstring without_volume_spec(const std::wstring& path);

} // namespace AFS_Core
