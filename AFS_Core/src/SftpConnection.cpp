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
#ifdef _UNIX
#include "SftpConnection.h"
#include "SshConnectTimeout.h"
#include "SshHostKeyVerification.h"
#include "Translation.h"
#include <map>
#include <stdexcept>

namespace AFS_Core {

#ifdef _TEST
std::atomic<int> sftp_connect_count{0};  // [SftpConnection.md#sftp_connect_count]
#endif

struct SftpConnection::Session {  // [SftpConnection.md#Session]
    ssh_session ssh = nullptr;
    sftp_session sftp = nullptr;
    ~Session()
    {
        if (sftp)
            sftp_free(sftp);
        if (ssh) {
            ssh_disconnect(ssh);
            ssh_free(ssh);
        }
    }
};

namespace {
    using SessionPool = std::map<std::string, std::shared_ptr<SftpConnection::Session>>;
    thread_local std::unique_ptr<SessionPool> pool;  // [SftpConnection.md#SshSessionScope]

    std::string pool_key(const SshAccount& account)
    {
        return account.name() + "|" + account.user() + "@" + account.host();
    }

    bool alive(const SftpConnection::Session& session)  // [SftpConnection.md#dropped-session]
    {
        return session.ssh != nullptr && ssh_is_connected(session.ssh);
    }
}

SshSessionScope::SshSessionScope()
    : outermost_(pool == nullptr)
{
    if (outermost_)
        pool = std::make_unique<SessionPool>();
}

SshSessionScope::~SshSessionScope()
{
    if (outermost_)
        pool.reset();
}

bool SshSessionScope::active()
{
    return pool != nullptr;
}

SftpConnection::SftpConnection(const SshAccount& account)
{
    connect(account);
}

SftpConnection::~SftpConnection() = default;

void SftpConnection::fail(const std::string& message)
{
    session_.reset();
    throw std::runtime_error(message);
}

void SftpConnection::unreachable(const std::string& message)
{
    session_.reset();
    throw SshUnreachable(message);
}

void SftpConnection::connect(const SshAccount& account)  // [SftpConnection.md#connect]
{
    if (pool) {
        const auto found = pool->find(pool_key(account));
        if (found != pool->end() && alive(*found->second)) {
            session_ = found->second;
            return;
        }
    }
    open_session(account);
    if (pool)
        (*pool)[pool_key(account)] = session_;
}

void SftpConnection::open_session(const SshAccount& account)  // [SftpConnection.md#libssh-message-ids]
{
#ifdef _TEST
    ++sftp_connect_count;
#endif
    session_ = std::make_shared<Session>();
    session_->ssh = ssh_new();
    if (!session_->ssh)
        fail(translation::translate_utf8("ssh_session_create_error"));
    ssh_options_set(session_->ssh, SSH_OPTIONS_HOST, account.host().c_str());
    const unsigned int port = account.port();
    ssh_options_set(session_->ssh, SSH_OPTIONS_PORT, &port);
    ssh_options_set(session_->ssh, SSH_OPTIONS_USER, account.user().c_str());
    apply_ssh_connect_timeout(session_->ssh);
    if (ssh_connect(session_->ssh) != SSH_OK)
        unreachable(translation::translate_utf8("libssh_connect_error") + error());
    const bool key_ok = account.host_key().empty() ? verify_ssh_host_key(session_->ssh, account.host())
        : verify_pinned_host_key(session_->ssh, account.host_key(), account.host());  // [SshHostKeyVerification.md#verify_pinned_host_key]
    if (!key_ok)
        fail(translation::translate_utf8("host_key_verification_failed"));
    if (ssh_userauth_password(session_->ssh, nullptr, account.password().c_str()) != SSH_AUTH_SUCCESS)
        fail(translation::translate_utf8("libssh_auth_error") + error());
    session_->sftp = sftp_new(session_->ssh);
    if (!session_->sftp)
        fail(translation::translate_utf8("sftp_session_create_error"));
    if (sftp_init(session_->sftp) != SSH_OK)
        fail(translation::translate_utf8("sftp_init_failed"));
}

ssh_session SftpConnection::ssh() const
{
    return session_ ? session_->ssh : nullptr;
}

sftp_session SftpConnection::sftp() const
{
    return session_ ? session_->sftp : nullptr;
}

std::string SftpConnection::error() const
{
    return ssh() ? ssh_get_error(ssh()) : std::string();
}

SftpFile SftpConnection::open(const std::string& path, int flags, mode_t mode) const
{
    return SftpFile(sftp_open(sftp(), path.c_str(), flags, mode), sftp_close);
}

SftpDir SftpConnection::open_dir(const std::string& path) const
{
    return SftpDir(sftp_opendir(sftp(), path.c_str()), sftp_closedir);
}

SftpAttributes SftpConnection::stat(const std::string& path) const
{
    return SftpAttributes(sftp_stat(sftp(), path.c_str()), sftp_attributes_free);
}

SftpAttributes SftpConnection::read_dir(sftp_dir dir) const
{
    return SftpAttributes(sftp_readdir(sftp(), dir), sftp_attributes_free);
}

} // namespace AFS_Core
#endif
