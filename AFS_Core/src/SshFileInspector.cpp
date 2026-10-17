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
#include "pch.h"
#include "SshFileInspector.h"
#include "SyncFileSsh.h"
#include "SshHostKeyVerification.h"
#include "SshConnectTimeout.h"
#include "Libssh2Init.h"
#include "Translation.h"
#include <string>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>
#include "Util.h"

#ifdef _UNIX
    #include <libssh/sftp.h>
#else
    #ifdef _WIN32
        #include <winsock2.h>
        #include <ws2tcpip.h>  // MinGW: link with -lws2_32 -lssh2 in Makefile/CMakeLists.txt
    #endif
#endif

namespace AFS_Core {

std::string time_to_iso(time_t t)  // [SshFileInspector.md#time_to_iso]
{
    struct tm timeinfo;
    #ifdef _WIN32
        localtime_s(&timeinfo, &t);
    #else
        localtime_r(&t, &timeinfo);
    #endif

    std::ostringstream oss;
    oss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

#ifdef _UNIX
SshFileInspector::SshFileInspector(const SshAccount& account)
    : connection_(account), account_(account)
{}

SshFileInspector::~SshFileInspector() = default;

RemoteFileInfo SshFileInspector::inspect_file(const std::string& remote_path)
{
    RemoteFileInfo info{};
    info.last_modified_iso = "N/A";
    const std::string sftp_path = win_to_sftp_path(remote_path);  // [SshFileInspector.md#path-separators]
    const auto attributes = connection_.stat(sftp_path);
    if (!attributes) {
        const int err = sftp_get_error(connection_.sftp());  // [SshFileInspector.md#absence-vs-failure]
        if (err == SSH_FX_NO_SUCH_FILE || err == SSH_FX_NO_SUCH_PATH)
            return info;
        throw std::runtime_error(translation::translate_utf8("sftp_stat_failed")
            + std::to_string(err) + "): " + sftp_path);
    }
    info.exists = true;
    info.size_bytes = attributes->size;
    info.raw_mtime = attributes->mtime;
    info.last_modified_iso = time_to_iso(info.raw_mtime);
    return info;
}

#else
#ifdef _WIN32

static void init_winsock()
{
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsadata;
        int err = WSAStartup(MAKEWORD(2, 2), &wsadata);
        if (err != 0) {
            throw std::runtime_error(translation::translate_utf8("wsastartup_failed"));
        }
        initialized = true;
    }
}

SshFileInspector::SshFileInspector(const SshAccount& account)  // [SshFileInspector.md#shared-message-ids]
    : account_(account)
{

    init_winsock();
    ensure_libssh2_initialized();

    struct addrinfo hints = {}, *result = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(account.host().c_str(), "22", &hints, &result) != 0) {
        throw SshUnreachable(translation::translate_utf8("host_name_not_resolvable") + account.host());
    }

    sock_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock_ == INVALID_SOCKET) {
        freeaddrinfo(result);
        throw std::runtime_error(translation::translate_utf8("socket_create_error"));
    }

    if (connect(sock_, result->ai_addr, (int)result->ai_addrlen) != 0) {
        freeaddrinfo(result);
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        throw SshUnreachable(translation::translate_utf8("server_connect_failed"));
    }
    freeaddrinfo(result);

    my_ssh_session_ = libssh2_session_init();
    if (!my_ssh_session_) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        throw std::runtime_error(translation::translate_utf8("ssh_session_create_error"));
    }
    apply_ssh_connect_timeout(my_ssh_session_);

    int rc = libssh2_session_handshake(my_ssh_session_, sock_);
    if (rc) {
        cleanup();
        throw std::runtime_error(translation::translate_utf8("ssh_handshake_failed"));
    }

    if (!verify_ssh_host_key(my_ssh_session_, account.host())) {
        cleanup();
        throw std::runtime_error(translation::translate_utf8("host_key_verification_failed"));
    }

    rc = libssh2_userauth_password(my_ssh_session_,
                                   account.user().c_str(),
                                   account.password().c_str());
    if (rc) {
        cleanup();
        throw std::runtime_error(translation::translate_utf8("ssh_auth_failed"));
    }

    sftp_session_ptr_ = libssh2_sftp_init(my_ssh_session_);
    if (!sftp_session_ptr_) {
        cleanup();
        throw std::runtime_error(translation::translate_utf8("sftp_init_failed"));
    }
}

SshFileInspector::~SshFileInspector()
{
    cleanup();
}

void SshFileInspector::cleanup()
{
    if (sftp_session_ptr_) {
        libssh2_sftp_shutdown(sftp_session_ptr_);
        sftp_session_ptr_ = nullptr;
    }
    if (my_ssh_session_) {
        libssh2_session_disconnect(my_ssh_session_, "Normal shutdown");
        libssh2_session_free(my_ssh_session_);
        my_ssh_session_ = nullptr;
    }
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
}

RemoteFileInfo SshFileInspector::inspect_file(const std::string& remote_path)
{
    RemoteFileInfo info{};
    info.exists = false;
    info.size_bytes = 0;
    info.raw_mtime = 0;
    info.last_modified_iso = "N/A";

    if (!sftp_session_ptr_) {
        return info;
    }
    auto sftp_path = remote_path;
#ifdef WIN32
    sftp_path = win_to_sftp_path(remote_path);  // [SshFileInspector.md#no-double-path-translation]
#endif
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftp_session_ptr_, sftp_path.c_str(), &attrs);

    if (rc != 0) {
        const unsigned long err = libssh2_sftp_last_error(sftp_session_ptr_);  // [SshFileInspector.md#absence-vs-failure]
        if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL
            && (err == LIBSSH2_FX_NO_SUCH_FILE || err == LIBSSH2_FX_NO_SUCH_PATH)) {
            info.exists = false;
            return info;
        }
        throw std::runtime_error(translation::translate_utf8("sftp_stat_failed")
            + std::to_string(err) + "): " + remote_path);
    }
#ifdef WIN32
    if (remote_path.empty()) {
        info.exists = false;
        return info;
	}
#endif
    info.exists = true;
    info.size_bytes = attrs.filesize;
    info.raw_mtime = static_cast<time_t>(attrs.mtime);
    info.last_modified_iso = time_to_iso(info.raw_mtime);

    return info;
}

#else  // fallback for unsupported platforms

SshFileInspector::SshFileInspector(const SshAccount& account)
    : account_(account)
{
    throw std::runtime_error(translation::translate_utf8("ssh_unsupported_platform"));
}

SshFileInspector::~SshFileInspector() {}

void SshFileInspector::cleanup() {}

RemoteFileInfo SshFileInspector::inspect_file(const std::string& remote_path)
{
    RemoteFileInfo info{};
    info.exists = false;
    info.size_bytes = 0;
    info.raw_mtime = 0;
    info.last_modified_iso = "N/A";
    return info;
}

#endif // _WIN32
#endif // _UNIX

} // namespace AFS_Core
