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
