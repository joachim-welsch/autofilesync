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

#ifdef _UNIX
    #include "SftpConnection.h"
#else
    #ifdef _WIN32
        #include <libssh2.h>
        #include <libssh2_sftp.h>
    #endif
#endif

#include "SshAccount.h"
#include "SshUnreachable.h"

namespace AFS_Core {

struct RemoteFileInfo {
    bool exists;
    uint64_t size_bytes;
    std::string last_modified_iso;
    time_t raw_mtime;
};

class SshFileInspector {
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
  SshFileInspector(const SshAccount& account);  // [SshFileInspector.md#SshFileInspector]
  ~SshFileInspector();
  RemoteFileInfo inspect_file(const std::string& remote_path);  // [SshFileInspector.md#inspect_file]
};

} // namespace AFS_Core
