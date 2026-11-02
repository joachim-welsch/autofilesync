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
#ifdef _WIN32
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#include "SshDirInspector.h"
#include "SshFileInspector.h"
#include "SshHostKeyVerification.h"
#include "SshConnectTimeout.h"
#include "Libssh2Init.h"
#include "Translation.h"
#include "FilesystemUtil.h"
#include "Logging.h"
#include "Util.h"
#include <string>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <cstring>

#ifdef _UNIX
    #include <libssh/sftp.h>
    #include <sys/stat.h>
#endif

namespace AFS_Core {

namespace {
    bool listed(const std::string& name, const std::string& remote_path)  // [SshDirInspector.md#entry-names]
    {
        if (name == "." || name == "..")
            return false;
        if (fsu::plain_entry_name(name))
            return true;
        log_error(translation::translate("remote_entry_name_rejected"), util::string_to_wstring(remote_path), util::string_to_wstring(name));
        return false;
    }
}

#ifdef _UNIX
namespace {
    std::string joined(const std::string& dir, const std::string& name)
    {
        return dir.empty() || dir.back() == '/' ? dir + name : dir + '/' + name;
    }

    RemoteDirEntry to_entry(const sftp_attributes_struct& attr, std::string name)
    {
        RemoteDirEntry entry{};
        entry.name = std::move(name);
        entry.is_directory = S_ISDIR(attr.permissions);
        entry.is_regular_file = S_ISREG(attr.permissions);
        entry.is_symlink = S_ISLNK(attr.permissions);
        entry.size_bytes = attr.size;
        entry.raw_mtime = attr.mtime;
        entry.permissions = attr.permissions;
        entry.last_modified_iso = time_to_iso(attr.mtime);
        return entry;
    }

    bool require_directory(const SftpConnection& connection, const std::string& remote_path, RemoteDirInfo& info)  // [SshDirInspector.md#require_directory]
    {
        info.path = remote_path;
        const auto attr = connection.stat(remote_path);
        if (!attr) {
            info.error_message = translation::translate_utf8("path_not_exist") + remote_path;
            return false;
        }
        info.exists = true;
        info.is_directory = S_ISDIR(attr->permissions);
        if (!info.is_directory)
            info.error_message = translation::translate_utf8("path_not_a_dir") + remote_path;
        return info.is_directory;
    }
}

SshDirInspector::SshDirInspector(const SshAccount& account)
    : connection_(account), account_(account)
{}

SshDirInspector::~SshDirInspector() = default;

RemoteDirInfo SshDirInspector::inspect_dir(const std::string& remote_path)
{
    RemoteDirInfo info{};
    if (!require_directory(connection_, remote_path, info))
        return info;
    const SftpDir dir = connection_.open_dir(remote_path);
    if (!dir) {
        info.error_message = translation::translate_utf8("remote_dir_open_error") + connection_.error();
        return info;
    }
    while (const auto attr = connection_.read_dir(dir.get()))
        if (listed(attr->name, remote_path))
            info.entries.push_back(to_entry(*attr, attr->name));
    if (!sftp_dir_eof(dir.get()))
        info.error_message = translation::translate_utf8("dir_read_error") + connection_.error();
    return info;
}

std::string SshDirInspector::get_home_dir()
{
    char* resolved = sftp_canonicalize_path(connection_.sftp(), ".");
    if (!resolved)
        throw std::runtime_error(translation::translate_utf8("home_dir_unknown") + connection_.error());
    std::string home(resolved);
    ssh_string_free_char(resolved);
    return home;
}

RemoteDirInfo SshDirInspector::inspect_dir_recursive(const std::string& remote_path)
{
    RemoteDirInfo result{};
    if (require_directory(connection_, remote_path, result))
        inspect_dir_recursive_impl(remote_path, result);
    return result;
}

void SshDirInspector::inspect_dir_recursive_impl(const std::string& remote_path, RemoteDirInfo& result)
{
    const SftpDir dir = connection_.open_dir(remote_path);
    if (!dir) {
        result.error_message = translation::translate_utf8("remote_dir_open_error") + remote_path;
        return;
    }
    while (const auto attr = connection_.read_dir(dir.get())) {
        if (!listed(attr->name, remote_path))
            continue;
        const std::string full_path = joined(remote_path, attr->name);
        result.entries.push_back(to_entry(*attr, full_path));
        if (S_ISDIR(attr->permissions) && !S_ISLNK(attr->permissions))
            inspect_dir_recursive_impl(full_path, result);
    }
    if (!sftp_dir_eof(dir.get()))  // [SshDirInspector.md#readdir-eof-vs-error]
        result.error_message = translation::translate_utf8("remote_dir_read_incomplete") + remote_path;
}

#else

#ifdef _WIN32

constexpr bool has_file_type(unsigned long permissions, unsigned long type)
{
    return (permissions & LIBSSH2_SFTP_S_IFMT) == type;
}

SshDirInspector::SshDirInspector(const SshAccount& account)
    : account_(account)
{
    WSADATA wsadata;
    int rc = WSAStartup(MAKEWORD(2, 0), &wsadata);
    if (rc != 0) {
        throw std::runtime_error(translation::translate_utf8("wsastartup_failed"));
    }

    struct sockaddr_in sin;
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ == INVALID_SOCKET) {
        WSACleanup();
        throw std::runtime_error(translation::translate_utf8("socket_create_error"));
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons(account_.port());
    sin.sin_addr.s_addr = inet_addr(account_.host().c_str());
    if (sin.sin_addr.s_addr == INADDR_NONE) {
        struct hostent* host = gethostbyname(account_.host().c_str());
        if (!host) {
            closesocket(sock_);
            WSACleanup();
            throw std::runtime_error(translation::translate_utf8("host_name_not_resolvable") + account_.host());
        }
        memcpy(&sin.sin_addr, host->h_addr, host->h_length);
    }

    if (connect(sock_, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
        closesocket(sock_);
        WSACleanup();
        throw std::runtime_error(translation::translate_utf8("server_connect_failed"));
    }

    ensure_libssh2_initialized();

    my_ssh_session_ = libssh2_session_init();
    if (!my_ssh_session_) {
        closesocket(sock_);
        WSACleanup();
        throw std::runtime_error(translation::translate_utf8("ssh_session_create_error"));
    }
    apply_ssh_connect_timeout(my_ssh_session_);

    rc = libssh2_session_handshake(my_ssh_session_, sock_);
    if (rc != 0) {
        cleanup();
        throw std::runtime_error(translation::translate_utf8("ssh_handshake_failed"));
    }

    if (!(account_.host_key().empty() ? verify_ssh_host_key(my_ssh_session_, account_.host(), account_.port()) : verify_pinned_host_key(my_ssh_session_, account_.host_key(), account_.host()))) {
        cleanup();
        throw std::runtime_error(translation::translate_utf8("host_key_verification_failed"));
    }

    rc = libssh2_userauth_password(my_ssh_session_, account_.user().c_str(), account_.password().c_str());
    if (rc != 0) {
        cleanup();
        throw std::runtime_error(translation::translate_utf8("ssh_auth_failed"));
    }

    sftp_session_ptr_ = libssh2_sftp_init(my_ssh_session_);
    if (!sftp_session_ptr_) {
        cleanup();
        throw std::runtime_error(translation::translate_utf8("sftp_init_failed"));
    }
}

SshDirInspector::~SshDirInspector()
{
    if (sftp_session_ptr_) {
        libssh2_sftp_shutdown(sftp_session_ptr_);
        sftp_session_ptr_ = nullptr;
    }
    cleanup();
}

void SshDirInspector::cleanup()
{
    if (my_ssh_session_) {
        libssh2_session_disconnect(my_ssh_session_, "Normal Shutdown");
        libssh2_session_free(my_ssh_session_);
        my_ssh_session_ = nullptr;
    }
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
    WSACleanup();
}

RemoteDirInfo SshDirInspector::inspect_dir(const std::string& remote_path)
{
    RemoteDirInfo info{};
    info.exists = false;
    info.is_directory = false;
    info.path = remote_path;

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftp_session_ptr_, remote_path.c_str(), &attrs);
    if (rc != 0) {
        info.error_message = translation::translate_utf8("path_not_exist") + remote_path;
        return info;
    }

    info.exists = true;
    info.is_directory = has_file_type(attrs.permissions, LIBSSH2_SFTP_S_IFDIR);

    if (!info.is_directory) {
        info.error_message = translation::translate_utf8("path_not_a_dir") + remote_path;
        return info;
    }

    LIBSSH2_SFTP_HANDLE* dir_handle = libssh2_sftp_opendir(sftp_session_ptr_, remote_path.c_str());
    if (!dir_handle) {
        char* err_msg;
        libssh2_session_last_error(my_ssh_session_, &err_msg, nullptr, 0);
        info.error_message = translation::translate_utf8("remote_dir_open_error") + std::string(err_msg);
        return info;
    }

    char buffer[512];
    LIBSSH2_SFTP_ATTRIBUTES entry_attrs;
    int read_rc = 0;

    while ((read_rc = libssh2_sftp_readdir(dir_handle, buffer, sizeof(buffer), &entry_attrs)) > 0) {
        std::string name = buffer;
        if (!listed(name, remote_path))
            continue;

        RemoteDirEntry entry{};
        entry.name = name;
        entry.is_directory = has_file_type(entry_attrs.permissions, LIBSSH2_SFTP_S_IFDIR);
        entry.is_regular_file = has_file_type(entry_attrs.permissions, LIBSSH2_SFTP_S_IFREG);
        entry.is_symlink = has_file_type(entry_attrs.permissions, LIBSSH2_SFTP_S_IFLNK);
        entry.size_bytes = entry_attrs.filesize;
        entry.raw_mtime = static_cast<time_t>(entry_attrs.mtime);
        entry.permissions = static_cast<uint32_t>(entry_attrs.permissions);

        entry.last_modified_iso = time_to_iso(entry.raw_mtime);

        info.entries.push_back(entry);
    }

    if (read_rc < 0)  // 0 is EOF, negative means an error in the middle of the listing
        info.error_message = translation::translate_utf8("remote_dir_read_incomplete") + remote_path;

    libssh2_sftp_closedir(dir_handle);
    return info;
}

std::string SshDirInspector::get_home_dir()
{
    char buffer[1024];
    int rc = libssh2_sftp_realpath(sftp_session_ptr_, ".", buffer, sizeof(buffer));
    if (rc < 0) {
        char* err_msg;
        libssh2_session_last_error(my_ssh_session_, &err_msg, nullptr, 0);
        throw std::runtime_error(translation::translate_utf8("home_dir_unknown") + std::string(err_msg));
    }
    return std::string(buffer, rc);
}

RemoteDirInfo SshDirInspector::inspect_dir_recursive(const std::string& remote_path)
{
    RemoteDirInfo result{};
    result.path = remote_path;
    result.exists = false;
    result.is_directory = false;

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftp_session_ptr_, remote_path.c_str(), &attrs);
    if (rc != 0) {
        result.error_message = translation::translate_utf8("path_not_exist") + remote_path;
        return result;
    }

    result.exists = true;
    result.is_directory = has_file_type(attrs.permissions, LIBSSH2_SFTP_S_IFDIR);

    if (!result.is_directory) {
        result.error_message = translation::translate_utf8("path_not_a_dir") + remote_path;
        return result;
    }

    inspect_dir_recursive_impl(remote_path, result);
    return result;
}

void SshDirInspector::inspect_dir_recursive_impl(const std::string& remote_path, RemoteDirInfo& result)
{
    LIBSSH2_SFTP_HANDLE* dir_handle = libssh2_sftp_opendir(sftp_session_ptr_, remote_path.c_str());
    if (!dir_handle) {
        result.error_message = translation::translate_utf8("remote_dir_open_error") + remote_path;
        return;
    }

    char buffer[512];
    LIBSSH2_SFTP_ATTRIBUTES entry_attrs;
    int read_rc = 0;

    while ((read_rc = libssh2_sftp_readdir(dir_handle, buffer, sizeof(buffer), &entry_attrs)) > 0) {
        std::string name = buffer;
        if (!listed(name, remote_path))
            continue;

        std::string full_path = remote_path;
        if (!full_path.empty() && full_path.back() != '/') {
            full_path += '/';
        }
        full_path += name;

        RemoteDirEntry entry{};
        entry.name = full_path;
        entry.is_directory = has_file_type(entry_attrs.permissions, LIBSSH2_SFTP_S_IFDIR);
        entry.is_regular_file = has_file_type(entry_attrs.permissions, LIBSSH2_SFTP_S_IFREG);
        entry.is_symlink = has_file_type(entry_attrs.permissions, LIBSSH2_SFTP_S_IFLNK);
        entry.size_bytes = entry_attrs.filesize;
        entry.raw_mtime = static_cast<time_t>(entry_attrs.mtime);
        entry.permissions = static_cast<uint32_t>(entry_attrs.permissions);

        entry.last_modified_iso = time_to_iso(entry.raw_mtime);

        result.entries.push_back(entry);

        if (entry.is_directory && !entry.is_symlink) {
            inspect_dir_recursive_impl(full_path, result);
        }
    }

    if (read_rc < 0)  // 0 is EOF, negative means an error in the middle of the listing
        result.error_message = translation::translate_utf8("remote_dir_read_incomplete") + remote_path;

    libssh2_sftp_closedir(dir_handle);
}

#endif // _WIN32
#endif // _UNIX

} // namespace AFS_Core
