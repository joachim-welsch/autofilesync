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
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <atomic>
#include <memory>
#include "SshAccount.h"
#include "SshUnreachable.h"
#endif

namespace AFS_Core {

#ifdef _UNIX
using SftpFile = std::unique_ptr<sftp_file_struct, int(*)(sftp_file)>;
using SftpDir = std::unique_ptr<sftp_dir_struct, int(*)(sftp_dir)>;
using SftpAttributes = std::unique_ptr<sftp_attributes_struct, void(*)(sftp_attributes)>;

class SftpConnection {  // [SftpConnection.md#SftpConnection]
public:
    struct Session;  // [SftpConnection.md#Session]
private:
    std::shared_ptr<Session> session_;
    [[noreturn]] void fail(const std::string& message);
    [[noreturn]] void unreachable(const std::string& message);
    void open_session(const SshAccount& account);
public:
    SftpConnection() = default;
    explicit SftpConnection(const SshAccount& account);
    SftpConnection(const SftpConnection&) = delete;
    SftpConnection& operator=(const SftpConnection&) = delete;
    ~SftpConnection();
    void connect(const SshAccount& account);  // [SftpConnection.md#connect]
    ssh_session ssh() const;
    sftp_session sftp() const;
    std::string error() const;
    SftpFile open(const std::string& path, int flags, mode_t mode) const;
    SftpDir open_dir(const std::string& path) const;
    SftpAttributes stat(const std::string& path) const;
    SftpAttributes read_dir(sftp_dir dir) const;
};

class SshSessionScope {  // [SftpConnection.md#SshSessionScope]
    const bool outermost_;
public:
    SshSessionScope();
    ~SshSessionScope();
    SshSessionScope(const SshSessionScope&) = delete;
    SshSessionScope& operator=(const SshSessionScope&) = delete;
    static bool active();
};

#ifdef _TEST
extern std::atomic<int> sftp_connect_count;  // [SftpConnection.md#sftp_connect_count]
#endif
#else
class SshSessionScope {  // [SftpConnection.md#SshSessionScope] the libssh2 modules open a session per operation still
public:
    static bool active() { return false; }
};
#endif

} // namespace AFS_Core
