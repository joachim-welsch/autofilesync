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
#include "SyncBase.h"
#include "FileDifference.h"
#include "SyncPlan.h"
#include <optional>
#ifdef _TEST
#include <atomic>
#endif

namespace AFS_Core {

class SyncFile: public SyncBase {
    bool transfer_filename_ = false;
    std::wstring state_key_;  // [SyncFile.md#state]

    bool copy_source_to_dest(const std::wstring& source, const std::wstring& destination, uint64_t size);
    bool copy_dest_to_source(const std::wstring& destination, const std::wstring& source, uint64_t size);
    bool save_copy_file(const std::wstring& src, const std::wstring& dest);
    CopyResult core_copy_function(const std::wstring& src, const std::wstring& dest);
    void pre_copy(const std::wstring& src, const std::wstring& dest);
    std::optional<PlannedAction> decide(const FileDifference& fd) const;  // [SyncFile.md#decide]
    bool empty_file_skipped(const FileDifference& fd, PlannedAction action) const;  // [SyncFile.md#empty-source-file]
    void sync_difference(const FileDifference& fd, const std::wstring& source, const std::wstring& destination, uint64_t filesize);
    void carry_out(PlannedAction action, const FileDifference& fd, const std::wstring& source, const std::wstring& destination, uint64_t filesize);
    bool perform(PlannedAction action, const FileDifference& fd, const std::wstring& source, const std::wstring& destination, uint64_t filesize);
    void update_state(PlannedAction action, const std::wstring& source, const std::wstring& destination);  // [SyncFile.md#state]
    void settle_state(const FileDifference& fd);
    void ensure_parent_dir(PlannedAction action, const std::wstring& source, const std::wstring& destination);  // [SyncFile.md#parent-of-a-kept-file]
    bool create_parent_dir(const std::wstring& path, bool ssh, bool in_source);
    bool rename_instead(PlannedAction action, const FileDifference& fd, const std::wstring& source, const std::wstring& destination);  // [SyncFile.md#renames]
    std::optional<std::wstring> rename_candidate(const FileDifference& fd, bool new_in_source) const;
    bool old_file_moved_away(const FileDifference& fd, const std::wstring& old_key, bool new_in_source, bool pending) const;
    bool rename_path(const std::wstring& from, const std::wstring& to, bool ssh, bool in_source);
    bool defer_deletion(PlannedAction action, const FileDifference& fd, const std::wstring& source, const std::wstring& destination);  // [SyncFile.md#deferred-deletions]
    bool resolve_conflict(PlannedAction action, const FileDifference& fd, const std::wstring& source, const std::wstring& destination);  // [SyncFile.md#conflict-copies]
    std::wstring conflict_copy_name(const std::wstring& path, bool ssh) const;  // [SyncFile.md#conflict-copy-name]
    void record_pair(const std::wstring& key, const std::wstring& source, const std::wstring& destination);
    bool delete_target_file(const std::wstring& source, const std::wstring& destination);
    bool delete_source_file(const std::wstring& source, const std::wstring& destination);
    void set_ssh();
    bool ssh_source() const;
    bool ssh_dest() const;
public:
    SyncFile(const SyncItem& si, std::size_t& copy_count, bool only_count = false, bool transfer_filename = false,
        SyncState* state = nullptr, const std::wstring& state_root = L"", const std::wstring& target_root = L"");
    void operator()();
    void carry_out_deferred(const PendingDeletion& deletion);  // [SyncFile.md#deferred-deletions]
#ifdef _TEST
    bool test_save_copy_file(const std::wstring& src, const std::wstring& dest) { return save_copy_file(src, dest); }
    void test_copy_dest_to_source(const std::wstring& destination, const std::wstring& source, uint64_t size = 0) { copy_dest_to_source(destination, source, size); }
#endif
};

#ifdef _TEST
extern std::atomic<int> wait_for_updated_filetime_call_count;
#endif

inline
bool SyncFile::ssh_dest() const { return ssh_ == SSHPATH::dest || ssh_ == SSHPATH::both; }

inline
bool SyncFile::ssh_source() const { return ssh_ == SSHPATH::source || ssh_ == SSHPATH::both; }

} // namespace AFS_Core
