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
