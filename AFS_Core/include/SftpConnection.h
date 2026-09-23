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
#ifdef _UNIX
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <memory>
#include <string>
#include "SshAccount.h"
#include "SshUnreachable.h"

namespace AFS_Core {

using SftpFile = std::unique_ptr<sftp_file_struct, int(*)(sftp_file)>;
using SftpDir = std::unique_ptr<sftp_dir_struct, int(*)(sftp_dir)>;
using SftpAttributes = std::unique_ptr<sftp_attributes_struct, void(*)(sftp_attributes)>;

class SftpConnection {  // owns one libssh session with its SFTP channel; both are released in the destructor
    ssh_session ssh_ = nullptr;
    sftp_session sftp_ = nullptr;
    void release();
    [[noreturn]] void fail(const std::string& message);
    [[noreturn]] void unreachable(const std::string& message);
public:
    SftpConnection() = default;
    explicit SftpConnection(const SshAccount& account);
    SftpConnection(const SftpConnection&) = delete;
    SftpConnection& operator=(const SftpConnection&) = delete;
    ~SftpConnection();
    void connect(const SshAccount& account);  // [SftpConnection.md#connect]
    ssh_session ssh() const { return ssh_; }
    sftp_session sftp() const { return sftp_; }
    std::string error() const;
    SftpFile open(const std::string& path, int flags, mode_t mode) const;
    SftpDir open_dir(const std::string& path) const;
    SftpAttributes stat(const std::string& path) const;
    SftpAttributes read_dir(sftp_dir dir) const;
};

} // namespace AFS_Core
#endif
