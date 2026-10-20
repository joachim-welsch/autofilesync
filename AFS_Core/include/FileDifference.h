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
#include <string>
#include <filesystem>
#include <optional>
#include "SyncItem.h"
#include "SyncState.h"

namespace AFS_Core {

struct ObservedFile {
    uint64_t size = 0;
    std::filesystem::file_time_type time{};
};
std::optional<ObservedFile> observe_file(const std::wstring& path, bool ssh);  // [FileDifference.md#observe_file]

class FileDifference {
public:
    enum class Result { Equal, SourceModified, DestModified, DestNotExists, SourceNotExits,
                        SourceDriveNotMounted, DestDriveNotMounted, ContentConflict,
                        SourceDeleted, DestDeleted, BothModified, NeitherExists, Undefined };  // [FileDifference.md#three-way]
private:
    const std::wstring  source_;
    const std::wstring  dest_;
    uint64_t            sourcesize_=0, destsize_=0;
    std::filesystem::file_time_type sourcetime_{}, desttime_{};
    CompareItems        ci_;
    SSHPATH             ssh_ {SSHPATH::none};
    bool                two_way_sync_ = false;
    const FileState*    base_ = nullptr;  // [FileDifference.md#three-way]
    Result              result_ = Result::Equal;
    bool newer(std::filesystem::file_time_type a, std::filesystem::file_time_type b) const;
    bool unchanged(uint64_t size, std::filesystem::file_time_type time, std::filesystem::file_time_type base_time) const;  // [FileDifference.md#unchanged]
    bool alike() const;
    Result source_missing(bool dest_absent) const;
    Result dest_missing() const;
    Result compare_with_base() const;
    Result compare_metadata(std::filesystem::file_time_type sourcetime, std::filesystem::file_time_type desttime) const;
    void compare_content(std::filesystem::file_time_type sourcetime, std::filesystem::file_time_type desttime);
public:
    FileDifference(const std::wstring& source, const std::wstring& dest, const CompareItems& ci, SSHPATH ssh = SSHPATH::none,
        bool two_way_sync = false, const FileState* base = nullptr);
    uint64_t operator()();
    Result result() const { return result_; }
    bool has_base() const { return base_ != nullptr; }
    bool same_time(std::filesystem::file_time_type a, std::filesystem::file_time_type b) const { return !newer(a, b) && !newer(b, a); }  // within the item's tolerance
    uint64_t source_size() const { return sourcesize_; }
    uint64_t dest_size() const { return destsize_; }
    std::filesystem::file_time_type source_time() const { return sourcetime_; }
    std::filesystem::file_time_type dest_time() const { return desttime_; }
};

} // namespace AFS_Core
