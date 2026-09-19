/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "SyncItem.h"
#include "Global.h"

namespace AFS_Core {

class SyncReport {
    uint64_t        bytes_transfered_ = 0;
    WStringVector   errors_;
public:
    static constexpr std::size_t MAX_ERRORS = 100;
    void transferred(uint64_t bytes) { bytes_transfered_ += bytes; }
    void error(const std::wstring& message);
    uint64_t bytes_transferd() const { return bytes_transfered_; }
    const WStringVector& errors() const { return errors_; }
    bool successful() const { return errors_.empty(); }
    bool empty() const { return bytes_transfered_ == 0 && errors_.empty(); }
};

class SyncReportGuard {  // [CopyLog.md#SyncReportGuard]
    SyncReport* previous_;
public:
    explicit SyncReportGuard(SyncReport* report);
    ~SyncReportGuard();
    SyncReportGuard(const SyncReportGuard&) = delete;
    SyncReportGuard& operator=(const SyncReportGuard&) = delete;
};

void report_sync_error(const std::wstring& message);  // [CopyLog.md#report_sync_error]
void report_sync_volume(uint64_t bytes);

bool write_copy_log(const std::filesystem::path& directory, const SyncItem& item,
    std::size_t files, const SyncReport& report);  // [CopyLog.md#write_copy_log]

std::filesystem::path copy_log_file_path(const std::filesystem::path& directory,
    const std::wstring& item_name);  // the proof file write_copy_log() appends an item's entries to

} // namespace AFS_Core
