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

#include <string>
#include <vector>
#ifdef _UNIX
#include "SftpConnection.h"
#else
    #ifdef _WIN32
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
    #include <winsock2.h>
    #include <libssh2.h>
    #include <libssh2_sftp.h>
    #endif
#endif
#include "SshAccount.h"

namespace AFS_Core {

struct RemoteDirEntry {
    std::string name;
    bool is_directory;
    bool is_regular_file;
    bool is_symlink;
    uint64_t size_bytes;
    std::string last_modified_iso;
    time_t raw_mtime;
    uint32_t permissions;
};

struct RemoteDirInfo {
    bool exists;
    bool is_directory;
    std::string path;
    std::vector<RemoteDirEntry> entries;
    std::string error_message;
};

class SshDirInspector {
#ifdef _UNIX
    SftpConnection connection_;
#else
    #ifdef _WIN32
    LIBSSH2_SESSION* my_ssh_session_ = nullptr;
    LIBSSH2_SFTP* sftp_session_ptr_ = nullptr;
    SOCKET sock_ = INVALID_SOCKET;
    void cleanup();
    #endif
#endif
    SshAccount account_;
public:
    SshDirInspector(const SshAccount& account);
    ~SshDirInspector();

    RemoteDirInfo inspect_dir(const std::string& remote_path);

    RemoteDirInfo inspect_dir_recursive(const std::string& remote_path);

    std::string get_home_dir();  // [SshDirInspector.md#get_home_dir]

private:
    void inspect_dir_recursive_impl(const std::string& remote_path, RemoteDirInfo& result);
};

} // namespace AFS_Core
