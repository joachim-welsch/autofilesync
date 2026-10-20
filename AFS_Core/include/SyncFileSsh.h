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
#include <optional>
#include <vector>
#include <atomic>
#include "SshAccount.h"
#include "SyncBase.h"

namespace AFS_Core {

struct SshDirEntry {
    std::wstring name;
    bool is_directory = false;
    bool is_regular_file = false;
    bool is_symlink = false;
    uint64_t size = 0;
};

std::optional<SshAccount> get_account_from_path(const std::wstring& path);
std::string get_remote_path(const std::wstring& path);
std::string win_to_sftp_path(const std::string& win);

CopyResult copy_ssh_to_fs(const std::wstring& src, const std::wstring& dest);
CopyResult copy_ssh_to_ssh(const std::wstring& src, const std::wstring& dest);
CopyResult copy_fs_to_ssh(const std::wstring& src, const std::wstring& dest);

bool ssh_exists(const std::wstring& path);
bool ssh_is_directory(const std::wstring& path);
bool ssh_mkdir(const std::wstring& path);
bool ssh_mkdir_p(const std::wstring& path);  // [SyncFileSsh.md#ssh_mkdir_p]
bool ssh_remove_file(const std::wstring& path);
bool ssh_rmdir(const std::wstring& path);  // [SyncFileSsh.md#ssh_rmdir]
bool ssh_rename(const std::wstring& from, const std::wstring& to);  // [SyncFileSsh.md#ssh_rename]
std::vector<SshDirEntry> ssh_list_dir(const std::wstring& path);

#ifdef _TEST
extern std::atomic<int> ssh_is_directory_call_count;  // [SyncFileSsh.md#ssh_is_directory_call_count]
#endif

} // namespace AFS_Core
