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

#include "SshAccount.h"
#include <functional>
#include <optional>
#include <string>

#ifdef _UNIX
    #include <libssh/libssh.h>
#else
    #ifdef _WIN32
        #include <libssh2.h>
    #endif
#endif

namespace AFS_Core {

using UnknownHostKeyDecision = std::function<bool(const std::string& host, const std::string& fingerprint)>;
void on_unknown_host_key(UnknownHostKeyDecision decision);  // [SshHostKeyVerification.md#unknown-host-key]

#ifdef _UNIX
bool verify_ssh_host_key(ssh_session session, const std::string& host);  // [SshHostKeyVerification.md#verify_ssh_host_key]
bool verify_pinned_host_key(ssh_session session, const std::string& expected_line, const std::string& host);  // [SshHostKeyVerification.md#verify_pinned_host_key]
#else
    #ifdef _WIN32
bool verify_ssh_host_key(LIBSSH2_SESSION* session, const std::string& host, unsigned short port = 22);
bool verify_pinned_host_key(LIBSSH2_SESSION* session, const std::string& expected_line, const std::string& host);  // [SshHostKeyVerification.md#verify_pinned_host_key]
    #endif
#endif

std::optional<std::string> fetch_ssh_host_key(const SshAccount& account, std::string& error);  // [SshHostKeyVerification.md#fetch_ssh_host_key]
std::string host_key_fingerprint(const std::string& host_key_line);  // [SshHostKeyVerification.md#host_key_fingerprint]

} // namespace AFS_Core
