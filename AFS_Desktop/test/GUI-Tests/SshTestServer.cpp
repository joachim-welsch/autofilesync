/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
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
#include "SshTestServer.h"
#include "SshHostKeyVerification.h"
#ifdef _WIN32
    #include "Libssh2Init.h"
    #include <ws2tcpip.h>
    #include <libssh2.h>
    #include <libssh2_sftp.h>
#else
    #include <libssh/libssh.h>
    #include <libssh/sftp.h>
    #include <fcntl.h>
#endif
#include <stdexcept>
#ifdef _WIN32
    #include <process.h>
#else
    #include <unistd.h>
#endif

using namespace AFS_Core;

namespace guitest {

SshAccount ssh_test_account(const std::string& name)
{
    SshAccount account;
    account.set_name(name);
    account.set_host(SSH_TEST_HOST);
    account.set_user(SSH_TEST_USER);
    account.set_password(SSH_TEST_PASSWORD);
    return account;
}

#ifdef _WIN32

struct RemoteSftp::Session {  // [SshTestServer.md#RemoteSftp]
    SOCKET socket_ = INVALID_SOCKET;
    LIBSSH2_SESSION* session = nullptr;
    LIBSSH2_SFTP* sftp = nullptr;
    bool winsock_started_ = false;

    Session()
    {
        try {
            open();
        } catch (...) {
            release();  // the destructor does not run for a throwing constructor
            throw;
        }
    }

    ~Session()
    {
        release();
    }

    void open()
    {
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
            throw std::runtime_error("WSAStartup fehlgeschlagen");
        winsock_started_ = true;
        connect_socket();
        ensure_libssh2_initialized();
        session = libssh2_session_init();
        if (session == nullptr)
            throw std::runtime_error("libssh2_session_init fehlgeschlagen");
        if (libssh2_session_handshake(session, socket_) != 0)
            fail("SSH-Handshake fehlgeschlagen");
        if (!verify_ssh_host_key(session, SSH_TEST_HOST))
            throw std::runtime_error("Host-Key-Pruefung fehlgeschlagen");
        if (libssh2_userauth_password(session, SSH_TEST_USER, SSH_TEST_PASSWORD) != 0)
            fail("Anmeldung fehlgeschlagen");
        sftp = libssh2_sftp_init(session);
        if (sftp == nullptr)
            fail("SFTP-Initialisierung fehlgeschlagen");
    }

    void connect_socket()
    {
        addrinfo hints{};
        hints.ai_family = AF_INET;  // same address family SshDirInspector connects with
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* address = nullptr;
        if (getaddrinfo(SSH_TEST_HOST, "22", &hints, &address) != 0)
            throw std::runtime_error("Host nicht aufloesbar: " + std::string(SSH_TEST_HOST));
        socket_ = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        const bool connected = socket_ != INVALID_SOCKET
            && ::connect(socket_, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0;
        freeaddrinfo(address);
        if (!connected)
            throw std::runtime_error("Verbindung zum SSH-Server fehlgeschlagen");
    }

    [[noreturn]] void fail(const char* what) const
    {
        char* message = nullptr;
        libssh2_session_last_error(session, &message, nullptr, 0);
        throw std::runtime_error(std::string(what) + ": " + (message ? message : ""));
    }

    void release()
    {
        if (sftp != nullptr)
            libssh2_sftp_shutdown(sftp);
        if (session != nullptr) {
            libssh2_session_disconnect(session, "Normal Shutdown");
            libssh2_session_free(session);
        }
        if (socket_ != INVALID_SOCKET)
            closesocket(socket_);
        if (winsock_started_)
            WSACleanup();
        sftp = nullptr;
        session = nullptr;
        socket_ = INVALID_SOCKET;
        winsock_started_ = false;
    }
};

std::string RemoteSftp::home_dir() const
{
    char buffer[1024];
    const int length = libssh2_sftp_realpath(session->sftp, ".", buffer, sizeof(buffer));
    if (length < 0)
        session->fail("Login-Verzeichnis nicht ermittelbar");
    return std::string(buffer, length);
}

std::string RemoteSftp::scratch_dir() const  // [SshTestServer.md#scratch_dir]
{
    return home_dir();
}

bool RemoteSftp::make_dir(const std::string& path) const
{
    return libssh2_sftp_mkdir(session->sftp, path.c_str(), 0755) == 0;
}

bool RemoteSftp::create_file(const std::string& path, const std::string& content) const
{
    LIBSSH2_SFTP_HANDLE* file = libssh2_sftp_open(session->sftp, path.c_str(),
        LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC, 0644);
    if (file == nullptr)
        return false;
    const bool written = content.empty() || libssh2_sftp_write(file, content.data(), content.size()) == static_cast<ssize_t>(content.size());
    libssh2_sftp_close(file);
    return written;
}

std::string RemoteSftp::read_file(const std::string& path) const
{
    LIBSSH2_SFTP_HANDLE* file = libssh2_sftp_open(session->sftp, path.c_str(), LIBSSH2_FXF_READ, 0);
    if (file == nullptr)
        return std::string();
    std::string content;
    char buffer[4096];
    for (ssize_t read = libssh2_sftp_read(file, buffer, sizeof(buffer)); read > 0; read = libssh2_sftp_read(file, buffer, sizeof(buffer)))
        content.append(buffer, read);
    libssh2_sftp_close(file);
    return content;
}

bool RemoteSftp::exists(const std::string& path) const
{
    LIBSSH2_SFTP_ATTRIBUTES attributes;
    return libssh2_sftp_stat(session->sftp, path.c_str(), &attributes) == 0;
}

void RemoteSftp::remove_file(const std::string& path) const
{
    libssh2_sftp_unlink(session->sftp, path.c_str());
}

void RemoteSftp::remove_dir(const std::string& path) const
{
    libssh2_sftp_rmdir(session->sftp, path.c_str());
}

#else

struct RemoteSftp::Session {  // [SshTestServer.md#RemoteSftp]
    ssh_session session = nullptr;
    sftp_session sftp = nullptr;

    Session()
    {
        session = ssh_new();
        if (session == nullptr)
            throw std::runtime_error("ssh_new fehlgeschlagen");
        ssh_options_set(session, SSH_OPTIONS_HOST, SSH_TEST_HOST);
        ssh_options_set(session, SSH_OPTIONS_USER, SSH_TEST_USER);
        if (ssh_connect(session) != SSH_OK)
            throw std::runtime_error(ssh_get_error(session));
        if (!verify_ssh_host_key(session, SSH_TEST_HOST))
            throw std::runtime_error("Host-Key-Pruefung fehlgeschlagen");
        if (ssh_userauth_password(session, nullptr, SSH_TEST_PASSWORD) != SSH_AUTH_SUCCESS)
            throw std::runtime_error(ssh_get_error(session));
        sftp = sftp_new(session);
        if (sftp == nullptr || sftp_init(sftp) != SSH_OK)
            throw std::runtime_error("SFTP-Initialisierung fehlgeschlagen");
    }

    ~Session()
    {
        if (sftp != nullptr)
            sftp_free(sftp);
        if (session != nullptr) {
            ssh_disconnect(session);
            ssh_free(session);
        }
    }
};

std::string RemoteSftp::home_dir() const
{
    char* path = sftp_canonicalize_path(session->sftp, ".");
    if (path == nullptr)
        throw std::runtime_error("Login-Verzeichnis nicht ermittelbar");
    std::string home(path);
    ssh_string_free_char(path);
    return home;
}

std::string RemoteSftp::scratch_dir() const  // [SshTestServer.md#scratch_dir]
{
    return "/tmp";
}

bool RemoteSftp::make_dir(const std::string& path) const
{
    return sftp_mkdir(session->sftp, path.c_str(), 0755) == SSH_OK;
}

bool RemoteSftp::create_file(const std::string& path, const std::string& content) const
{
    sftp_file file = sftp_open(session->sftp, path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file == nullptr)
        return false;
    const bool written = content.empty() || sftp_write(file, content.data(), content.size()) == static_cast<ssize_t>(content.size());
    sftp_close(file);
    return written;
}

std::string RemoteSftp::read_file(const std::string& path) const
{
    sftp_file file = sftp_open(session->sftp, path.c_str(), O_RDONLY, 0);
    if (file == nullptr)
        return std::string();
    std::string content;
    char buffer[4096];
    for (ssize_t read = sftp_read(file, buffer, sizeof(buffer)); read > 0; read = sftp_read(file, buffer, sizeof(buffer)))
        content.append(buffer, read);
    sftp_close(file);
    return content;
}

bool RemoteSftp::exists(const std::string& path) const
{
    sftp_attributes attributes = sftp_stat(session->sftp, path.c_str());
    if (attributes == nullptr)
        return false;
    sftp_attributes_free(attributes);
    return true;
}

void RemoteSftp::remove_file(const std::string& path) const
{
    sftp_unlink(session->sftp, path.c_str());
}

void RemoteSftp::remove_dir(const std::string& path) const
{
    sftp_rmdir(session->sftp, path.c_str());
}

#endif

RemoteSftp::RemoteSftp(): session(std::make_unique<Session>()) {}

RemoteSftp::~RemoteSftp() = default;

namespace {

int current_process_id()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

}

void SshGuiTest::SetUp()
{
    GuiTest::SetUp();
    try {
        sftp_ = std::make_unique<RemoteSftp>();
    } catch (const std::exception& ex) {
        GTEST_SKIP() << "Kein SSH-Testserver auf " << SSH_TEST_HOST << " erreichbar: " << ex.what();
    }
    probe_dir_ = sftp_->scratch_dir() + "/autofilesync_guitest_" + std::to_string(current_process_id());  // [SshTestServer.md#probe_dir]
    sftp_->remove_dir(probe_dir_);
    if (!sftp_->make_dir(probe_dir_))
        FAIL() << "Das Testverzeichnis auf dem Server ließ sich nicht anlegen: " << probe_dir_;
}

void SshGuiTest::TearDown()
{
    if (sftp_ != nullptr && !probe_dir_.empty())
        sftp_->remove_dir(probe_dir_);  // [SshTestServer.md#probe_dir]
    sftp_.reset();
    GuiTest::TearDown();
}

}
