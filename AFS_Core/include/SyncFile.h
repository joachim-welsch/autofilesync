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
#include "SyncBase.h"
#include "FileDifference.h"
#ifdef _TEST
#include <atomic>
#endif

namespace AFS_Core {

class SyncFile: public SyncBase {
    bool transfer_filename_ = false;

    void copy_source_to_dest(const std::wstring& source, const std::wstring& destination, uint64_t size);
    void copy_dest_to_source(const std::wstring& destination, const std::wstring& source, uint64_t size);
    bool save_copy_file(const std::wstring& src, const std::wstring& dest);
    CopyResult core_copy_function(const std::wstring& src, const std::wstring& dest);
    void pre_copy(const std::wstring& src, const std::wstring& dest);
    void sync_difference(const FileDifference& fd, const std::wstring& source, const std::wstring& destination, uint64_t filesize);
    void delete_target_file(const std::wstring& source, const std::wstring& destination);
    void set_ssh();
    bool ssh_source() const;
    bool ssh_dest() const;
public:
    SyncFile(const SyncItem& si, std::size_t& copy_count, bool only_count = false, bool transfer_filename = false);
    void operator()();
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
