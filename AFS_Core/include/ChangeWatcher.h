/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
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
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace AFS_Core {

struct WatchedDir {  // [ChangeWatcher.md#WatchedDir]
    std::wstring item;
    std::filesystem::path dir;
    bool recursive;
};

class ChangeWatcher {  // [ChangeWatcher.md#ChangeWatcher]
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    explicit ChangeWatcher(const std::vector<WatchedDir>& dirs);
    ~ChangeWatcher();
    ChangeWatcher(const ChangeWatcher&) = delete;
    ChangeWatcher& operator=(const ChangeWatcher&) = delete;
    bool watching(const std::wstring& item) const;  // [ChangeWatcher.md#watching]
    std::optional<TimePoint> last_change(const std::wstring& item) const;
    void clear(const std::wstring& item);  // [ChangeWatcher.md#clear]
    static bool available();  // [ChangeWatcher.md#available]
private:
    struct Backend;  // [ChangeWatcher.md#Backend]
    struct Entry {
        bool watching = false;
        std::optional<TimePoint> last_change;
    };
    mutable std::mutex mutex_;
    std::map<std::wstring, Entry> items_;
    std::unique_ptr<Backend> backend_;
    void changed(const std::wstring& item);
    void changed_everywhere();  // [ChangeWatcher.md#overflow]
    void lost(const std::wstring& item, const std::filesystem::path& dir);  // [ChangeWatcher.md#watching]
};

} // namespace AFS_Core
