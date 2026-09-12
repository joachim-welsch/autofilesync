#include "pch.h"
#ifdef _UNIX
#include "SftpConnection.h"
#include "SshConnectTimeout.h"
#include "SshHostKeyVerification.h"
#include "Translation.h"
#include <stdexcept>

namespace AFS_Core {

SftpConnection::SftpConnection(const SshAccount& account)
{
    connect(account);
}

SftpConnection::~SftpConnection()
{
    release();
}

void SftpConnection::release()
{
    if (sftp_)
        sftp_free(sftp_);
    if (ssh_) {
        ssh_disconnect(ssh_);
        ssh_free(ssh_);
    }
    sftp_ = nullptr;
    ssh_ = nullptr;
}

void SftpConnection::fail(const std::string& message)
{
    release();
    throw std::runtime_error(message);
}

void SftpConnection::connect(const SshAccount& account)  // [SftpConnection.md#libssh-message-ids]
{
    ssh_ = ssh_new();
    if (!ssh_)
        fail(translation::translate_utf8("ssh_session_create_error"));
    ssh_options_set(ssh_, SSH_OPTIONS_HOST, account.host().c_str());
    ssh_options_set(ssh_, SSH_OPTIONS_USER, account.user().c_str());
    apply_ssh_connect_timeout(ssh_);
    if (ssh_connect(ssh_) != SSH_OK)
        fail(translation::translate_utf8("libssh_connect_error") + error());
    if (!verify_ssh_host_key(ssh_, account.host()))
        fail(translation::translate_utf8("host_key_verification_failed"));
    if (ssh_userauth_password(ssh_, nullptr, account.password().c_str()) != SSH_AUTH_SUCCESS)
        fail(translation::translate_utf8("libssh_auth_error") + error());
    sftp_ = sftp_new(ssh_);
    if (!sftp_)
        fail(translation::translate_utf8("sftp_session_create_error"));
    if (sftp_init(sftp_) != SSH_OK)
        fail(translation::translate_utf8("sftp_init_failed"));
}

std::string SftpConnection::error() const
{
    return ssh_get_error(ssh_);
}

SftpFile SftpConnection::open(const std::string& path, int flags, mode_t mode) const
{
    return SftpFile(sftp_open(sftp_, path.c_str(), flags, mode), sftp_close);
}

SftpDir SftpConnection::open_dir(const std::string& path) const
{
    return SftpDir(sftp_opendir(sftp_, path.c_str()), sftp_closedir);
}

SftpAttributes SftpConnection::stat(const std::string& path) const
{
    return SftpAttributes(sftp_stat(sftp_, path.c_str()), sftp_attributes_free);
}

SftpAttributes SftpConnection::read_dir(sftp_dir dir) const
{
    return SftpAttributes(sftp_readdir(sftp_, dir), sftp_attributes_free);
}

} // namespace AFS_Core
#endif
