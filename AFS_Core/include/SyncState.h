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
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace AFS_Core {

struct FileState {  // [SyncState.md#FileState]
    uint64_t size = 0;
    std::filesystem::file_time_type source_time{};
    std::filesystem::file_time_type target_time{};
    bool operator==(const FileState&) const = default;
};

using FileStates = std::map<std::wstring, FileState>;  // keyed by the path, see [SyncState.md#path]

struct PendingDeletion {  // [SyncState.md#pass-notebook]
    std::wstring key;
    std::wstring source;
    std::wstring target;
    bool in_target;
    uint64_t bytes;
};

struct PendingDirRemoval {
    std::wstring path;
    bool ssh;
};

class SyncState {  // [SyncState.md#SyncState]
    std::wstring source_, target_;
    FileStates files_;
    std::multimap<uint64_t, std::wstring> by_size_;  // [SyncState.md#by_size_]
    std::set<std::wstring> seen_;
    bool changed_ = false;
    std::vector<PendingDeletion> pending_deletions_;  // [SyncState.md#pass-notebook]
    std::vector<PendingDirRemoval> pending_dir_removals_;
    std::set<std::wstring> renamed_;
    bool mass_deletion_approved_ = false;  // [SyncState.md#mass-deletion-approval]
    void unindex(const std::wstring& path);
public:
    static constexpr int FORMAT_VERSION = 1;  // [SyncState.md#FORMAT_VERSION]
    SyncState() = default;
    SyncState(std::wstring source, std::wstring target, FileStates files = {});
    const std::wstring& source() const { return source_; }
    const std::wstring& target() const { return target_; }
    const FileStates& files() const { return files_; }
    bool empty() const { return files_.empty(); }
    bool changed() const { return changed_; }  // [SyncState.md#changed]
    const FileState* find(const std::wstring& path) const;
    bool knows_below(const std::wstring& dir) const;  // [SyncState.md#knows_below]
    std::vector<std::wstring> with_size(uint64_t size) const;  // [SyncState.md#by_size_]
    void defer(PendingDeletion deletion);  // [SyncState.md#pass-notebook]
    std::optional<PendingDeletion> claim(const std::wstring& key);
    void renamed(const std::wstring& key);  // [SyncState.md#renamed]
    std::vector<PendingDeletion> take_deletions();
    void defer(PendingDirRemoval removal);
    std::vector<PendingDirRemoval> take_dir_removals();
    void approve_mass_deletion() { mass_deletion_approved_ = true; }  // [SyncState.md#mass-deletion-approval]
    bool mass_deletion_approved() const { return mass_deletion_approved_; }
    void record(const std::wstring& path, const FileState& state);  // [SyncState.md#record]
    void seen(const std::wstring& path);
    void forget(const std::wstring& path);
    void forget_unseen();  // [SyncState.md#forget_unseen]
};

std::filesystem::path sync_state_dir();  // [SyncState.md#sync_state_dir]
std::filesystem::path sync_state_file_path(const std::filesystem::path& directory, const std::wstring& item_name);  // [SyncState.md#sync_state_file_path]
std::optional<SyncState> read_sync_state(const std::filesystem::path& file, const std::wstring& source, const std::wstring& target);  // [SyncState.md#read_sync_state]
bool write_sync_state(const std::filesystem::path& file, const SyncState& state);  // [SyncState.md#write_sync_state]
bool remove_sync_state(const std::filesystem::path& file);

} // namespace AFS_Core
