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
#include "SyncFileSsh.h"
#include "Translation.h"
#include "SshHostKeyVerification.h"
#include "SshConnectTimeout.h"
#include "Libssh2Init.h"
#include "SftpConnection.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <memory>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _UNIX
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#else
#ifdef _WIN32
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <libssh2.h>
    #include <libssh2_sftp.h>
    #pragma comment(lib, "ws2_32.lib")
#endif
#endif
#include "Config.h"
#include "Util.h"
#include "Logging.h"
#include "Synchronize.h"

namespace AFS_Core {

using std::wstring;
using util::string_to_wstring;
using util::wstring_to_string;

#ifdef _TEST
std::atomic<int> ssh_is_directory_call_count{0};
#endif

std::string win_to_sftp_path(const std::string& win)
{
    std::string p = win;
    std::replace(p.begin(), p.end(), '\\', '/');
    if (p.size() >= 2 && std::isalpha(p[0]) && p[1] == ':')  // "C:/..." becomes "/C:/..."
        p = "/" + p;
    return p;
}

namespace {
    void keep_local_time(const wstring& local_path, std::time_t mtime)  // [SyncFileSsh.md#transfer-keeps-the-modification-time]
    {
        std::error_code ec;
        std::filesystem::last_write_time(local_path, util::to_file_time(mtime), ec);
        if (ec)
            log_error(translation::translate("file_time_not_settable"), local_path, string_to_wstring(ec.message()));
    }

    std::time_t local_time_of(const wstring& path)
    {
        std::error_code ec;
        const auto time = std::filesystem::last_write_time(path, ec);
        return ec ? 0 : util::to_time_t(time);
    }
}

#ifdef _WIN32
namespace {
    bool sftp_write_all(LIBSSH2_SFTP_HANDLE* file, const char* data, size_t length)  // [SyncFileSsh.md#short-write]
    {
        while (length > 0) {
            ssize_t written = libssh2_sftp_write(file, data, length);
            if (written <= 0)
                return false;
            data += written;
            length -= static_cast<size_t>(written);
        }
        return true;
    }

    void keep_remote_time(LIBSSH2_SFTP* sftp, const std::string& remote_path, std::time_t mtime)  // [SyncFileSsh.md#transfer-keeps-the-modification-time]
    {
        LIBSSH2_SFTP_ATTRIBUTES attributes{};
        attributes.flags = LIBSSH2_SFTP_ATTR_ACMODTIME;
        attributes.atime = static_cast<unsigned long>(mtime);
        attributes.mtime = static_cast<unsigned long>(mtime);
        if (libssh2_sftp_setstat(sftp, remote_path.c_str(), &attributes) != 0)
            log_error(translation::translate("file_time_not_settable"), string_to_wstring(remote_path));
    }

    class WinsockInit {
    public:
        WinsockInit()
        {
            WSADATA wsadata;
            WSAStartup(MAKEWORD(2, 0), &wsadata);
        }
        ~WinsockInit()
        {
            WSACleanup();
        }
    };

    static WinsockInit winsock_init;

    bool resolve_host(const std::string& host, sockaddr_in& sin)  // [SyncFileSsh.md#resolve_host]
    {
        sin = {};
        sin.sin_family = AF_INET;
        sin.sin_port = htons(22);
#ifdef _WIN32
        int pton_result = inet_pton(AF_INET, host.c_str(), &sin.sin_addr);
        if (pton_result == 1) {
            return true;
        }
#else
        sin.sin_addr.s_addr = inet_addr(host.c_str());  // first try to parse the text as an IP address
        if (sin.sin_addr.s_addr != INADDR_NONE) {
            return true;
        }
#endif
        addrinfo hints{};  // if not a valid IP, try to resolve it as a hostname
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* result = nullptr;
        int rc = getaddrinfo(host.c_str(), "22", &hints, &result);
        if (rc != 0 || result == nullptr) {
            return false;
        }

        sin = *reinterpret_cast<sockaddr_in*>(result->ai_addr);
        freeaddrinfo(result);
        return true;
    }
}
#endif

std::optional<SshAccount> get_account_from_path(const wstring& path)
{
    auto colon_pos = path.find(L':');
    if (colon_pos == wstring::npos || colon_pos == 0) {
        return std::nullopt;
    }
    std::string ssh_name = wstring_to_string(path.substr(0, colon_pos));
    return Config::instance().find_ssh_account_by_name(ssh_name);
}

std::string get_remote_path(const wstring& path)
{
    auto colon_pos = path.find(L':');
    if (colon_pos == wstring::npos) {
        return wstring_to_string(path);
    }
    return wstring_to_string(path.substr(colon_pos + 1));
}

#ifdef _UNIX
namespace {
    constexpr size_t SFTP_BUFFER_SIZE = 65536;  // 64 KiB per request stays within what every SFTP server accepts
    constexpr int WRITE_FLAGS = O_WRONLY | O_CREAT | O_TRUNC;
    constexpr mode_t DIRECTORY_MODE = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

    bool failed(const wstring& message)
    {
        log_error(message);
        return false;
    }

    CopyResult copy_failed(const wstring& message)
    {
        log_error(message);
        return CopyResult::Error;
    }

    std::optional<std::string> connect_to(const wstring& path, SftpConnection& connection)  // [SyncFileSsh.md#connect_to]
    {
        const auto account = get_account_from_path(path);
        if (!account) {
            log_error(translation::translate("ssh_account_not_found_for") + path);
            return std::nullopt;
        }
        try {
            connection.connect(*account);
        } catch (const SshUnreachable& e) {
            log_warning(L"SSH: " + string_to_wstring(e.what()));  // [SyncFileSsh.md#unreachable-is-a-warning]
            return std::nullopt;
        } catch (const std::exception& e) {
            log_error(L"SSH: " + string_to_wstring(e.what()));
            return std::nullopt;
        }
        return get_remote_path(path);
    }

    bool sftp_write_all(sftp_file file, const char* data, size_t length)  // [SyncFileSsh.md#short-write]
    {
        while (length > 0) {
            ssize_t written = sftp_write(file, data, length);
            if (written <= 0)
                return false;
            data += written;
            length -= static_cast<size_t>(written);
        }
        return true;
    }

    CopyResult download(sftp_file remote_file, std::ofstream& local_file)
    {
        std::vector<char> buffer(SFTP_BUFFER_SIZE);
        ssize_t bytes_read;
        while ((bytes_read = sftp_read(remote_file, buffer.data(), buffer.size())) > 0) {
            if (Synchronize::stopping())
                return CopyResult::Stopped;
            if (!local_file.write(buffer.data(), bytes_read))
                return copy_failed(translation::translate("local_file_write_error"));
        }
        return bytes_read < 0 ? copy_failed(translation::translate("remote_file_read_error")) : CopyResult::OK;
    }

    CopyResult upload(std::ifstream& local_file, sftp_file remote_file)
    {
        std::vector<char> buffer(SFTP_BUFFER_SIZE);
        while (local_file.read(buffer.data(), buffer.size()) || local_file.gcount() > 0) {
            if (Synchronize::stopping())
                return CopyResult::Stopped;
            if (!sftp_write_all(remote_file, buffer.data(), static_cast<size_t>(local_file.gcount())))
                return copy_failed(translation::translate("remote_file_write_error"));
        }
        return local_file.bad() ? copy_failed(translation::translate("local_file_read_error")) : CopyResult::OK;
    }

    CopyResult transfer(sftp_file src_file, sftp_file dest_file)
    {
        std::vector<char> buffer(SFTP_BUFFER_SIZE);
        ssize_t bytes_read;
        while ((bytes_read = sftp_read(src_file, buffer.data(), buffer.size())) > 0) {
            if (Synchronize::stopping())
                return CopyResult::Stopped;
            if (!sftp_write_all(dest_file, buffer.data(), static_cast<size_t>(bytes_read)))
                return copy_failed(translation::translate("target_file_write_error"));
        }
        return bytes_read < 0 ? copy_failed(translation::translate("source_file_read_error")) : CopyResult::OK;
    }

    constexpr uint32_t PERMISSION_BITS = 0777;  // [SyncFileSsh.md#copy-from-ssh-keeps-the-permissions]

    void keep_local_permissions(const wstring& local_path, uint32_t permissions)  // [SyncFileSsh.md#copy-from-ssh-keeps-the-permissions]
    {
        std::error_code ec;
        std::filesystem::permissions(local_path, static_cast<std::filesystem::perms>(permissions & PERMISSION_BITS), ec);
        if (ec)
            log_error(translation::translate("file_attributes_change"), local_path, string_to_wstring(ec.message()));
    }

    void keep_remote_permissions(const SftpConnection& connection, const std::string& remote_path, uint32_t permissions)  // [SyncFileSsh.md#copy-from-ssh-keeps-the-permissions]
    {
        sftp_attributes_struct attributes{};
        attributes.flags = SSH_FILEXFER_ATTR_PERMISSIONS;
        attributes.permissions = (permissions & PERMISSION_BITS) | S_IWUSR;  // [SyncFileSsh.md#remote-target-stays-writable]
        if (sftp_setstat(connection.sftp(), remote_path.c_str(), &attributes) != SSH_OK)
            log_error(translation::translate("file_attributes_change"), string_to_wstring(remote_path), string_to_wstring(connection.error()));
    }

    void keep_remote_time(const SftpConnection& connection, const std::string& remote_path, std::time_t mtime)  // [SyncFileSsh.md#transfer-keeps-the-modification-time]
    {
        sftp_attributes_struct attributes{};
        attributes.flags = SSH_FILEXFER_ATTR_ACMODTIME;
        attributes.atime = static_cast<uint32_t>(mtime);
        attributes.mtime = static_cast<uint32_t>(mtime);
        if (sftp_setstat(connection.sftp(), remote_path.c_str(), &attributes) != SSH_OK)
            log_error(translation::translate("file_time_not_settable"), string_to_wstring(remote_path));
    }

    SshDirEntry to_dir_entry(const sftp_attributes_struct& attr)
    {
        SshDirEntry entry;
        entry.name = string_to_wstring(attr.name);
        entry.is_directory = attr.type == SSH_FILEXFER_TYPE_DIRECTORY;
        entry.is_regular_file = attr.type == SSH_FILEXFER_TYPE_REGULAR;
        entry.is_symlink = attr.type == SSH_FILEXFER_TYPE_SYMLINK;
        entry.size = attr.size;
        return entry;
    }

    bool is_dot_entry(const char* name)
    {
        return std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0;
    }
}

CopyResult copy_ssh_to_fs(const wstring& src, const wstring& dest)
{
    log_debug(L"copy_ssh_to_fs: " + src + L" -> " + dest);
    SftpConnection connection;
    const auto remote_path = connect_to(src, connection);
    if (!remote_path)
        return CopyResult::Error;
    const SftpFile remote_file = connection.open(*remote_path, O_RDONLY, 0);
    if (!remote_file)
        return copy_failed(translation::translate("remote_file_open_error") + string_to_wstring(*remote_path));
    const SftpAttributes attributes = connection.stat(*remote_path);
    CopyResult result = CopyResult::Error;
    {
        std::ofstream local_file(wstring_to_string(dest), std::ios::binary);
        if (!local_file.is_open())
            return copy_failed(translation::translate("local_file_open_error") + dest);
        result = download(remote_file.get(), local_file);
    }
    if (result == CopyResult::OK && attributes) {  // [SyncFileSsh.md#transfer-keeps-the-modification-time]
        keep_local_permissions(dest, attributes->permissions);
        keep_local_time(dest, attributes->mtime);
    }
    return result;
}

CopyResult copy_ssh_to_ssh(const wstring& src, const wstring& dest)
{
    log_debug(L"copy_ssh_to_ssh: " + src + L" -> " + dest);
    SftpConnection source, target;
    const auto src_path = connect_to(src, source);
    if (!src_path)
        return CopyResult::Error;
    const auto dest_path = connect_to(dest, target);
    if (!dest_path)
        return CopyResult::Error;
    const SftpFile src_file = source.open(*src_path, O_RDONLY, 0);
    if (!src_file)
        return copy_failed(translation::translate("source_file_open_error") + string_to_wstring(*src_path));
    const SftpAttributes attributes = source.stat(*src_path);
    CopyResult result = CopyResult::Error;
    {
        const SftpFile dest_file = target.open(*dest_path, WRITE_FLAGS, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (!dest_file)
            return copy_failed(translation::translate("target_file_open_error") + string_to_wstring(*dest_path));
        result = transfer(src_file.get(), dest_file.get());
    }
    if (result == CopyResult::OK && attributes) {  // [SyncFileSsh.md#transfer-keeps-the-modification-time]
        keep_remote_permissions(target, *dest_path, attributes->permissions);
        keep_remote_time(target, *dest_path, attributes->mtime);
    }
    return result;
}

CopyResult copy_fs_to_ssh(const wstring& src, const wstring& dest)
{
    log_debug(L"copy_fs_to_ssh: " + src + L" -> " + dest);
    std::ifstream local_file(wstring_to_string(src), std::ios::binary);
    if (!local_file.is_open())
        return copy_failed(translation::translate("local_file_open_error") + src);
    SftpConnection connection;
    const auto remote_path = connect_to(dest, connection);
    if (!remote_path)
        return CopyResult::Error;
    CopyResult result = CopyResult::Error;
    {
        const SftpFile remote_file = connection.open(*remote_path, WRITE_FLAGS, S_IRUSR | S_IWUSR);
        if (!remote_file)
            return copy_failed(translation::translate("remote_file_open_write_error") + string_to_wstring(*remote_path));
        result = upload(local_file, remote_file.get());
    }
    if (result == CopyResult::OK)  // [SyncFileSsh.md#transfer-keeps-the-modification-time]
        keep_remote_time(connection, *remote_path, local_time_of(src));
    return result;
}

bool ssh_exists(const std::wstring& path)
{
    log_debug(L"ssh_exists: " + path);
    SftpConnection connection;
    const auto remote_path = connect_to(path, connection);
    return remote_path && connection.stat(*remote_path) != nullptr;
}

bool ssh_is_directory(const std::wstring& path)
{
    log_debug(L"ssh_is_directory: " + path);
#ifdef _TEST
    ++ssh_is_directory_call_count;
#endif
    SftpConnection connection;
    const auto remote_path = connect_to(path, connection);
    if (!remote_path)
        return false;
    const auto attr = connection.stat(*remote_path);
    return attr && attr->type == SSH_FILEXFER_TYPE_DIRECTORY;
}

bool ssh_mkdir(const std::wstring& path)
{
    log_debug(L"ssh_mkdir: " + path);
    SftpConnection connection;
    const auto remote_path = connect_to(path, connection);
    if (!remote_path)
        return false;
    if (sftp_mkdir(connection.sftp(), remote_path->c_str(), DIRECTORY_MODE) != SSH_OK)
        return failed(translation::translate("remote_dir_create_error") + string_to_wstring(*remote_path));
    return true;
}

bool ssh_remove_file(const std::wstring& path)
{
    log_debug(L"ssh_remove_file: " + path);
    SftpConnection connection;
    const auto remote_path = connect_to(path, connection);
    if (!remote_path)
        return false;
    if (sftp_unlink(connection.sftp(), remote_path->c_str()) != SSH_OK)
        return failed(translation::translate("remote_file_delete_error") + string_to_wstring(*remote_path));
    return true;
}

std::vector<SshDirEntry> ssh_list_dir(const std::wstring& path)
{
    log_debug(L"ssh_list_dir: " + path);
    std::vector<SshDirEntry> entries;
    SftpConnection connection;
    const auto remote_path = connect_to(path, connection);
    if (!remote_path)
        return entries;
    const SftpDir dir = connection.open_dir(*remote_path);
    if (!dir) {
        log_error(translation::translate("remote_dir_open_error") + string_to_wstring(*remote_path));
        return entries;
    }
    while (const auto attr = connection.read_dir(dir.get()))
        if (!is_dot_entry(attr->name))
            entries.push_back(to_dir_entry(*attr));
    if (!sftp_dir_eof(dir.get()))  // [SyncFileSsh.md#readdir-eof-vs-error]
        log_error(translation::translate("remote_dir_read_incomplete") + string_to_wstring(*remote_path) + L" - " + string_to_wstring(connection.error()));
    return entries;
}
#else
#ifdef _WIN32
CopyResult copy_ssh_to_fs(const wstring& src, const wstring& dest)
{
    log_debug(L"copy_ssh_to_fs: " + src + L" -> " + dest);
    
    auto account_opt = get_account_from_path(src);
    if (!account_opt.has_value()) {
        log_error(translation::translate("ssh_account_not_found_for") + src);
        return CopyResult::Error;
    }
    const SshAccount& account = account_opt.value();
    std::string remote_path = get_remote_path(src);

    ensure_libssh2_initialized();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        log_error(translation::translate("socket_create_error"));
        return CopyResult::Error;
    }

    sockaddr_in sin{};
    if (!resolve_host(account.host(), sin)) {
        log_warning(translation::translate("host_name_not_resolvable") + string_to_wstring(account.host()));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(sock);
        return CopyResult::Error;
    }

    if (connect(sock, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) != 0) {
        log_warning(translation::translate("ssh_connect_error_for_host") + string_to_wstring(account.host()));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(sock);
        return CopyResult::Error;
    }

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        log_error(translation::translate("ssh_session_create_error"));
        closesocket(sock);
        return CopyResult::Error;
    }
    apply_ssh_connect_timeout(session);
    
    if (libssh2_session_handshake(session, sock) != 0) {
        log_error(translation::translate("ssh_handshake_failed"));
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    if (!verify_ssh_host_key(session, account.host())) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    if (libssh2_userauth_password(session, account.user().c_str(), account.password().c_str()) != 0) {
        log_error(translation::translate("ssh_auth_failed"));
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    LIBSSH2_SFTP* sftp_session = libssh2_sftp_init(session);
    if (!sftp_session) {
        log_error(translation::translate("sftp_session_create_error"));
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    LIBSSH2_SFTP_HANDLE* remote_file = libssh2_sftp_open(sftp_session, remote_path.c_str(),
        LIBSSH2_FXF_READ, 0);
    if (!remote_file) {
        log_error(translation::translate("remote_file_open_error") + string_to_wstring(remote_path));
        libssh2_sftp_shutdown(sftp_session);
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }
    
    std::string dest_path = wstring_to_string(dest);
    std::ofstream local_file(dest_path, std::ios::binary);
    if (!local_file.is_open()) {
        log_error(translation::translate("local_file_open_error") + dest);
        libssh2_sftp_close(remote_file);
        libssh2_sftp_shutdown(sftp_session);
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }
    
    std::vector<char> buffer(65536);
    ssize_t bytes_read;
    CopyResult result = CopyResult::OK;
    
    while ((bytes_read = libssh2_sftp_read(remote_file, buffer.data(), buffer.size())) > 0) {
        if (Synchronize::stopping()) {
            log_debug(L"Stopped");
            result = CopyResult::Stopped;
            break;
        }
        local_file.write(buffer.data(), bytes_read);
        if (!local_file.good()) {
            log_error(translation::translate("local_file_write_error"));
            result = CopyResult::Error;
            break;
        }
    }
    
    if (bytes_read < 0) {
        log_error(translation::translate("remote_file_read_error"));
        result = CopyResult::Error;
    }

    LIBSSH2_SFTP_ATTRIBUTES attributes{};
    const bool attributes_read = libssh2_sftp_fstat(remote_file, &attributes) == 0;

    local_file.close();
    if (result == CopyResult::OK && attributes_read)  // [SyncFileSsh.md#transfer-keeps-the-modification-time]
        keep_local_time(dest, static_cast<std::time_t>(attributes.mtime));
    libssh2_sftp_close(remote_file);
    libssh2_sftp_shutdown(sftp_session);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);
    
    return result;
}

CopyResult copy_ssh_to_ssh(const wstring& src, const wstring& dest)
{
    log_debug(L"copy_ssh_to_ssh: " + src + L" -> " + dest);
    
    auto src_account_opt = get_account_from_path(src);
    if (!src_account_opt.has_value()) {
        log_error(translation::translate("ssh_account_not_found_source") + src);
        return CopyResult::Error;
    }
    const SshAccount& src_account = src_account_opt.value();
    std::string src_remote_path = get_remote_path(src);
    
    auto dest_account_opt = get_account_from_path(dest);
    if (!dest_account_opt.has_value()) {
        log_error(translation::translate("ssh_account_not_found_target") + dest);
        return CopyResult::Error;
    }
    const SshAccount& dest_account = dest_account_opt.value();
    std::string dest_remote_path = get_remote_path(dest);

    ensure_libssh2_initialized();
    SOCKET src_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (src_sock == INVALID_SOCKET) {
        log_error(translation::translate("source_socket_create_error"));
        return CopyResult::Error;
    }

    sockaddr_in src_sin{};
    if (!resolve_host(src_account.host(), src_sin)) {
        log_warning(translation::translate("source_host_name_resolve_error") + string_to_wstring(src_account.host()));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(src_sock);
        return CopyResult::Error;
    }

    if (connect(src_sock, (struct sockaddr*)&src_sin, sizeof(struct sockaddr_in)) != 0) {
        log_warning(translation::translate("ssh_connect_error_source"));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(src_sock);
        return CopyResult::Error;
    }

    LIBSSH2_SESSION* src_session = libssh2_session_init();
    if (!src_session) {
        log_error(translation::translate("source_ssh_session_create_error"));
        closesocket(src_sock);
        return CopyResult::Error;
    }
    apply_ssh_connect_timeout(src_session);
    
    if (libssh2_session_handshake(src_session, src_sock) != 0) {
        log_error(translation::translate("ssh_handshake_error_source"));
        libssh2_session_free(src_session);
        closesocket(src_sock);
        return CopyResult::Error;
    }

    if (!verify_ssh_host_key(src_session, src_account.host())) {
        libssh2_session_disconnect(src_session, "Normal Shutdown");
        libssh2_session_free(src_session);
        closesocket(src_sock);
        return CopyResult::Error;
    }

    if (libssh2_userauth_password(src_session, src_account.user().c_str(), src_account.password().c_str()) != 0) {
        log_error(translation::translate("ssh_auth_error_source"));
        libssh2_session_disconnect(src_session, "Normal Shutdown");
        libssh2_session_free(src_session);
        closesocket(src_sock);
        return CopyResult::Error;
    }
    
    LIBSSH2_SFTP* src_sftp_session = libssh2_sftp_init(src_session);
    if (!src_sftp_session) {
        log_error(translation::translate("source_sftp_session_create_error"));
        libssh2_session_disconnect(src_session, "Normal Shutdown");
        libssh2_session_free(src_session);
        closesocket(src_sock);
        return CopyResult::Error;
    }
    
    SOCKET dest_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (dest_sock == INVALID_SOCKET) {
        log_error(translation::translate("target_socket_create_error"));
        libssh2_sftp_shutdown(src_sftp_session);
        libssh2_session_disconnect(src_session, "Normal Shutdown");
        libssh2_session_free(src_session);
        closesocket(src_sock);
        return CopyResult::Error;
    }
    
    sockaddr_in dest_sin{};
    if (!resolve_host(dest_account.host(), dest_sin)) {
        log_warning(translation::translate("target_host_name_resolve_error") + string_to_wstring(dest_account.host()));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(dest_sock);
        libssh2_sftp_shutdown(src_sftp_session);
        libssh2_session_disconnect(src_session, "Normal Shutdown");
        libssh2_session_free(src_session);
        closesocket(src_sock);
        return CopyResult::Error;
    }

    if (connect(dest_sock, (struct sockaddr*)&dest_sin, sizeof(struct sockaddr_in)) != 0) {
        log_warning(translation::translate("ssh_connect_error_target"));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(dest_sock);
        libssh2_sftp_shutdown(src_sftp_session);
        libssh2_session_disconnect(src_session, "Normal Shutdown");
        libssh2_session_free(src_session);
        closesocket(src_sock);
        return CopyResult::Error;
    }

    LIBSSH2_SESSION* dest_session = libssh2_session_init();
    if (!dest_session) {
        log_error(translation::translate("target_ssh_session_create_error"));
        closesocket(dest_sock);
        libssh2_sftp_shutdown(src_sftp_session);
        libssh2_session_disconnect(src_session, "Normal Shutdown");
        libssh2_session_free(src_session);
        closesocket(src_sock);
        return CopyResult::Error;
    }
    apply_ssh_connect_timeout(dest_session);
    
    if (libssh2_session_handshake(dest_session, dest_sock) != 0) {
        log_error(translation::translate("ssh_handshake_error_target"));
        libssh2_session_free(dest_session);
        closesocket(dest_sock);
        libssh2_sftp_shutdown(src_sftp_session);
        libssh2_session_disconnect(src_session, "Normal Shutdown");
        libssh2_session_free(src_session);
        closesocket(src_sock);
        return CopyResult::Error;
    }

    if (!verify_ssh_host_key(dest_session, dest_account.host())) {
        libssh2_session_disconnect(dest_session, "Normal Shutdown");
        libssh2_session_free(dest_session);
        closesocket(dest_sock);
        libssh2_sftp_shutdown(src_sftp_session);
        libssh2_session_disconnect(src_session, "Normal Shutdown");
        libssh2_session_free(src_session);
        closesocket(src_sock);
        return CopyResult::Error;
    }

    if (libssh2_userauth_password(dest_session, dest_account.user().c_str(), dest_account.password().c_str()) != 0) {
        log_error(translation::translate("ssh_auth_error_target"));
        libssh2_session_disconnect(dest_session, "Normal Shutdown");
        libssh2_session_free(dest_session);
        closesocket(dest_sock);
        libssh2_sftp_shutdown(src_sftp_session);
        libssh2_session_disconnect(src_session, "Normal Shutdown");
        libssh2_session_free(src_session);
        closesocket(src_sock);
        return CopyResult::Error;
    }
    
    LIBSSH2_SFTP* dest_sftp_session = libssh2_sftp_init(dest_session);
    if (!dest_sftp_session) {
        log_error(translation::translate("target_sftp_session_create_error"));
        libssh2_session_disconnect(dest_session, "Normal Shutdown");
        libssh2_session_free(dest_session);
        closesocket(dest_sock);
        libssh2_sftp_shutdown(src_sftp_session);
        libssh2_session_disconnect(src_session, "Normal Shutdown");
        libssh2_session_free(src_session);
        closesocket(src_sock);
        return CopyResult::Error;
    }
    
    LIBSSH2_SFTP_HANDLE* src_file = libssh2_sftp_open(src_sftp_session, src_remote_path.c_str(),
        LIBSSH2_FXF_READ, 0);
    if (!src_file) {
        log_error(translation::translate("source_file_open_error") + string_to_wstring(src_remote_path));
        libssh2_sftp_shutdown(dest_sftp_session);
        libssh2_session_disconnect(dest_session, "Normal Shutdown");
        libssh2_session_free(dest_session);
        closesocket(dest_sock);
        libssh2_sftp_shutdown(src_sftp_session);
        libssh2_session_disconnect(src_session, "Normal Shutdown");
        libssh2_session_free(src_session);
        closesocket(src_sock);
        return CopyResult::Error;
    }
    
    LIBSSH2_SFTP_HANDLE* dest_file = libssh2_sftp_open(dest_sftp_session, dest_remote_path.c_str(),
        LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
        LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH);
    if (!dest_file) {
        log_error(translation::translate("target_file_open_error") + string_to_wstring(dest_remote_path));
        libssh2_sftp_close(src_file);
        libssh2_sftp_shutdown(dest_sftp_session);
        libssh2_session_disconnect(dest_session, "Normal Shutdown");
        libssh2_session_free(dest_session);
        closesocket(dest_sock);
        libssh2_sftp_shutdown(src_sftp_session);
        libssh2_session_disconnect(src_session, "Normal Shutdown");
        libssh2_session_free(src_session);
        closesocket(src_sock);
        return CopyResult::Error;
    }
    
    std::vector<char> buffer(65536);
    ssize_t bytes_read;
    CopyResult result = CopyResult::OK;
    
    while ((bytes_read = libssh2_sftp_read(src_file, buffer.data(), buffer.size())) > 0) {
        if (Synchronize::stopping()) {
            log_debug(L"Stopped");
            result = CopyResult::Stopped;
            break;
        }
        if (!sftp_write_all(dest_file, buffer.data(), static_cast<size_t>(bytes_read))) {
            log_error(translation::translate("target_file_write_error"));
            result = CopyResult::Error;
            break;
        }
    }
    
    if (bytes_read < 0) {
        log_error(translation::translate("source_file_read_error"));
        result = CopyResult::Error;
    }

    LIBSSH2_SFTP_ATTRIBUTES attributes{};
    const bool attributes_read = libssh2_sftp_fstat(src_file, &attributes) == 0;

    libssh2_sftp_close(dest_file);
    if (result == CopyResult::OK && attributes_read)  // [SyncFileSsh.md#transfer-keeps-the-modification-time]
        keep_remote_time(dest_sftp_session, dest_remote_path, static_cast<std::time_t>(attributes.mtime));
    libssh2_sftp_close(src_file);
    libssh2_sftp_shutdown(dest_sftp_session);
    libssh2_session_disconnect(dest_session, "Normal Shutdown");
    libssh2_session_free(dest_session);
    closesocket(dest_sock);
    libssh2_sftp_shutdown(src_sftp_session);
    libssh2_session_disconnect(src_session, "Normal Shutdown");
    libssh2_session_free(src_session);
    closesocket(src_sock);
    
    return result;
}

CopyResult copy_fs_to_ssh(const wstring& src, const wstring& dest)
{
    log_debug(L"copy_fs_to_ssh: " + src + L" -> " + dest);
    
    auto account_opt = get_account_from_path(dest);
    if (!account_opt.has_value()) {
        log_error(translation::translate("ssh_account_not_found_for") + dest);
        return CopyResult::Error;
    }
    const SshAccount& account = account_opt.value();
    std::string remote_path = get_remote_path(dest);
    
    std::string src_path = wstring_to_string(src);
    std::ifstream local_file(src_path, std::ios::binary);
    if (!local_file.is_open()) {
        log_error(translation::translate("local_file_open_error") + src);
        return CopyResult::Error;
    }

    ensure_libssh2_initialized();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        log_error(translation::translate("socket_create_error"));
        return CopyResult::Error;
    }

    sockaddr_in sin{};
    if (!resolve_host(account.host(), sin)) {
        log_warning(translation::translate("host_name_not_resolvable") + string_to_wstring(account.host()));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(sock);
        return CopyResult::Error;
    }

    if (connect(sock, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) != 0) {
        log_warning(translation::translate("server_connect_failed"));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(sock);
        return CopyResult::Error;
    }

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        log_error(translation::translate("ssh_session_create_error"));
        closesocket(sock);
        return CopyResult::Error;
    }
    apply_ssh_connect_timeout(session);

    if (libssh2_session_handshake(session, sock) != 0) {
        log_error(translation::translate("ssh_handshake_failed"));
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    if (!verify_ssh_host_key(session, account.host())) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    if (libssh2_userauth_password(session, account.user().c_str(), account.password().c_str()) != 0) {
        log_error(translation::translate("ssh_auth_failed"));
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    LIBSSH2_SFTP* sftp_session = libssh2_sftp_init(session);
    if (!sftp_session) {
        log_error(translation::translate("sftp_session_create_error"));
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }

    LIBSSH2_SFTP_HANDLE* remote_file = libssh2_sftp_open(sftp_session, remote_path.c_str(),
        LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
        LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR);
    if (!remote_file) {
        log_error(translation::translate("remote_file_open_write_error") + string_to_wstring(remote_path));
        libssh2_sftp_shutdown(sftp_session);
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return CopyResult::Error;
    }
    std::vector<char> buffer(65536);
    CopyResult result = CopyResult::OK;
    
    while (local_file.read(buffer.data(), buffer.size()) || local_file.gcount() > 0) {
        if (Synchronize::stopping()) {
            log_debug(L"Stopped");
            result = CopyResult::Stopped;
            break;
        }
        std::streamsize bytes_read = local_file.gcount();
        if (!sftp_write_all(remote_file, buffer.data(), static_cast<size_t>(bytes_read))) {
            log_error(translation::translate("remote_file_write_error"));
            result = CopyResult::Error;
            break;
        }
    }
    
    if (result == CopyResult::OK && local_file.bad()) {
        log_error(translation::translate("local_file_read_error"));
        result = CopyResult::Error;
    }
    
    local_file.close();
    libssh2_sftp_close(remote_file);
    if (result == CopyResult::OK)  // [SyncFileSsh.md#transfer-keeps-the-modification-time]
        keep_remote_time(sftp_session, remote_path, local_time_of(src));
    libssh2_sftp_shutdown(sftp_session);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);
    
    return result;
}

bool ssh_exists(const std::wstring& path)
{
    log_debug(L"ssh_exists: " + path);

    auto account_opt = get_account_from_path(path);
    if (!account_opt.has_value()) {
        log_error(translation::translate("ssh_account_not_found_for") + path);
        return false;
    }
    const SshAccount& account = account_opt.value();
    std::string remote_path = get_remote_path(path);

    ensure_libssh2_initialized();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        log_error(translation::translate("socket_create_error"));
        return false;
    }

    sockaddr_in sin{};
    if (!resolve_host(account.host(), sin)) {
        log_warning(translation::translate("host_name_not_resolvable") + string_to_wstring(account.host()));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(sock);
        return false;
    }

    if (connect(sock, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) != 0) {
        log_warning(translation::translate("server_connect_failed"));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(sock);
        return false;
    }

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        log_error(translation::translate("ssh_session_create_error"));
        closesocket(sock);
        return false;
    }
    apply_ssh_connect_timeout(session);

    if (libssh2_session_handshake(session, sock) != 0) {
        log_error(translation::translate("ssh_handshake_failed"));
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (!verify_ssh_host_key(session, account.host())) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (libssh2_userauth_password(session, account.user().c_str(), account.password().c_str()) != 0) {
        log_error(translation::translate("ssh_auth_failed"));
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    LIBSSH2_SFTP* sftp_session = libssh2_sftp_init(session);
    if (!sftp_session) {
        log_error(translation::translate("sftp_session_create_error"));
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftp_session, remote_path.c_str(), &attrs);
    bool exists = (rc == 0);

    libssh2_sftp_shutdown(sftp_session);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);

    return exists;
}

bool ssh_is_directory(const std::wstring& path)
{
    log_debug(L"ssh_is_directory: " + path);
#ifdef _TEST
    ++ssh_is_directory_call_count;
#endif

    auto account_opt = get_account_from_path(path);
    if (!account_opt.has_value()) {
        log_error(translation::translate("ssh_account_not_found_for") + path);
        return false;
    }
    const SshAccount& account = account_opt.value();
    std::string remote_path = get_remote_path(path);

    ensure_libssh2_initialized();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        log_error(translation::translate("socket_create_error"));
        return false;
    }

    sockaddr_in sin{};
    if (!resolve_host(account.host(), sin)) {
        log_warning(translation::translate("host_name_not_resolvable") + string_to_wstring(account.host()));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(sock);
        return false;
    }

    if (connect(sock, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) != 0) {
        log_warning(translation::translate("server_connect_failed"));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(sock);
        return false;
    }

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        log_error(translation::translate("ssh_session_create_error"));
        closesocket(sock);
        return false;
    }
    apply_ssh_connect_timeout(session);

    if (libssh2_session_handshake(session, sock) != 0) {
        log_error(translation::translate("ssh_handshake_failed"));
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (!verify_ssh_host_key(session, account.host())) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (libssh2_userauth_password(session, account.user().c_str(), account.password().c_str()) != 0) {
        log_error(translation::translate("ssh_auth_failed"));
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    LIBSSH2_SFTP* sftp_session = libssh2_sftp_init(session);
    if (!sftp_session) {
        log_error(translation::translate("sftp_session_create_error"));
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(sftp_session, remote_path.c_str(), &attrs);
    bool is_dir = rc == 0 && LIBSSH2_SFTP_S_ISDIR(attrs.permissions);

    libssh2_sftp_shutdown(sftp_session);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);

    return is_dir;
}

bool ssh_mkdir(const std::wstring& path)
{
    log_debug(L"ssh_mkdir: " + path);
    auto account_opt = get_account_from_path(path);
    if (!account_opt.has_value()) {
        log_error(translation::translate("ssh_account_not_found_for") + path);
        return false;
    }
    const SshAccount& account = account_opt.value();
    std::string remote_path = win_to_sftp_path(get_remote_path(path));

    ensure_libssh2_initialized();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        log_error(translation::translate("socket_create_error"));
        return false;
    }

    sockaddr_in sin{};
    if (!resolve_host(account.host(), sin)) {
        log_warning(translation::translate("host_name_not_resolvable") + string_to_wstring(account.host()));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(sock);
        return false;
    }

    if (connect(sock, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) != 0) {
        log_warning(translation::translate("server_connect_failed"));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(sock);
        return false;
    }

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        log_error(translation::translate("ssh_session_create_error"));
        closesocket(sock);
        return false;
    }
    apply_ssh_connect_timeout(session);

    if (libssh2_session_handshake(session, sock) != 0) {
        log_error(translation::translate("ssh_handshake_failed"));
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (!verify_ssh_host_key(session, account.host())) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (libssh2_userauth_password(session, account.user().c_str(), account.password().c_str()) != 0) {
        log_error(translation::translate("ssh_auth_failed"));
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    LIBSSH2_SFTP* sftp_session = libssh2_sftp_init(session);
    if (!sftp_session) {
        log_error(translation::translate("sftp_session_create_error"));
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    int rc = libssh2_sftp_mkdir(sftp_session, remote_path.c_str(),
        LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP | LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH);
    bool success = (rc == 0);
    if (!success)
        log_error(translation::translate("remote_dir_create_error") + string_to_wstring(remote_path));
    
    libssh2_sftp_shutdown(sftp_session);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);

    return success;
}

bool ssh_remove_file(const std::wstring& path)
{
    log_debug(L"ssh_remove_file: " + path);
    auto account_opt = get_account_from_path(path);
    if (!account_opt.has_value()) {
        log_error(translation::translate("ssh_account_not_found_for") + path);
        return false;
    }
    const SshAccount& account = account_opt.value();
    std::string remote_path = win_to_sftp_path(get_remote_path(path));

    ensure_libssh2_initialized();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        log_error(translation::translate("socket_create_error"));
        return false;
    }

    sockaddr_in sin{};
    if (!resolve_host(account.host(), sin)) {
        log_warning(translation::translate("host_name_not_resolvable") + string_to_wstring(account.host()));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(sock);
        return false;
    }

    if (connect(sock, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) != 0) {
        log_warning(translation::translate("server_connect_failed"));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(sock);
        return false;
    }

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        log_error(translation::translate("ssh_session_create_error"));
        closesocket(sock);
        return false;
    }
    apply_ssh_connect_timeout(session);

    if (libssh2_session_handshake(session, sock) != 0) {
        log_error(translation::translate("ssh_handshake_failed"));
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (!verify_ssh_host_key(session, account.host())) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    if (libssh2_userauth_password(session, account.user().c_str(), account.password().c_str()) != 0) {
        log_error(translation::translate("ssh_auth_failed"));
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    LIBSSH2_SFTP* sftp_session = libssh2_sftp_init(session);
    if (!sftp_session) {
        log_error(translation::translate("sftp_session_create_error"));
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }

    int rc = libssh2_sftp_unlink(sftp_session, remote_path.c_str());
    bool success = (rc == 0);
    if (!success) {
        log_error(translation::translate("remote_file_delete_error") + string_to_wstring(remote_path));
    }

    libssh2_sftp_shutdown(sftp_session);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);

    return success;
}

std::vector<SshDirEntry> ssh_list_dir(const std::wstring& path)
{
    log_debug(L"ssh_list_dir: " + path);
    std::vector<SshDirEntry> entries;
    
    auto account_opt = get_account_from_path(path);
    if (!account_opt.has_value()) {
        log_error(translation::translate("ssh_account_not_found_for") + path);
        return entries;
    }
    const SshAccount& account = account_opt.value();
    std::string remote_path = get_remote_path(path);

    ensure_libssh2_initialized();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        log_error(translation::translate("socket_create_error"));
        return entries;
    }

    sockaddr_in sin{};
    if (!resolve_host(account.host(), sin)) {
        log_warning(translation::translate("host_name_not_resolvable") + string_to_wstring(account.host()));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(sock);
        return entries;
    }

    if (connect(sock, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) != 0) {
        log_warning(translation::translate("server_connect_failed"));  // [SyncFileSsh.md#unreachable-is-a-warning]
        closesocket(sock);
        return entries;
    }

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) {
        log_error(translation::translate("ssh_session_create_error"));
        closesocket(sock);
        return entries;
    }
    apply_ssh_connect_timeout(session);

    if (libssh2_session_handshake(session, sock) != 0) {
        log_error(translation::translate("ssh_handshake_failed"));
        libssh2_session_free(session);
        closesocket(sock);
        return entries;
    }

    if (!verify_ssh_host_key(session, account.host())) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return entries;
    }

    if (libssh2_userauth_password(session, account.user().c_str(), account.password().c_str()) != 0) {
        log_error(translation::translate("ssh_auth_failed"));
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return entries;
    }

    LIBSSH2_SFTP* sftp_session = libssh2_sftp_init(session);
    if (!sftp_session) {
        log_error(translation::translate("sftp_session_create_error"));
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return entries;
    }
    
    LIBSSH2_SFTP_HANDLE* dir = libssh2_sftp_opendir(sftp_session, remote_path.c_str());
    if (!dir) {
        log_error(translation::translate("remote_dir_open_error") + string_to_wstring(remote_path));
        libssh2_sftp_shutdown(sftp_session);
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
        closesocket(sock);
        return entries;
    }
    
    char buffer[512];
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = 0;
    while ((rc = libssh2_sftp_readdir(dir, buffer, sizeof(buffer), &attrs)) > 0) {
        std::string name = buffer;
        if (name != "." && name != "..") {
            SshDirEntry entry;
            entry.name = string_to_wstring(name);
            entry.is_directory = LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
            entry.is_regular_file = LIBSSH2_SFTP_S_ISREG(attrs.permissions);
            entry.is_symlink = LIBSSH2_SFTP_S_ISLNK(attrs.permissions);
            entry.size = attrs.filesize;
            entries.push_back(entry);
        }
    }
    if (rc < 0)  // 0 is EOF, negative is an error mid-listing - see the UNIX variant above
        log_error(translation::translate("remote_dir_read_incomplete") + string_to_wstring(remote_path));

    libssh2_sftp_closedir(dir);
    libssh2_sftp_shutdown(sftp_session);
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);
    
    return entries;
}

#endif
#endif

} // namespace AFS_Core
