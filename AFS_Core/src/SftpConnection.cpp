#include "pch.h"
#ifdef _UNIX
#include "SftpConnection.h"
#include "SshConnectTimeout.h"
#include "SshHostKeyVerification.h"
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

void SftpConnection::connect(const SshAccount& account)
{
    ssh_ = ssh_new();
    if (!ssh_)
        fail("Fehler beim Erstellen der SSH Session.");
    ssh_options_set(ssh_, SSH_OPTIONS_HOST, account.host().c_str());
    ssh_options_set(ssh_, SSH_OPTIONS_USER, account.user().c_str());
    apply_ssh_connect_timeout(ssh_);
    if (ssh_connect(ssh_) != SSH_OK)
        fail("Verbindungsfehler: " + error());
    if (!verify_ssh_host_key(ssh_, account.host()))
        fail("Host-Key-Prüfung fehlgeschlagen: mögliche Man-in-the-Middle-Attacke.");
    if (ssh_userauth_password(ssh_, nullptr, account.password().c_str()) != SSH_AUTH_SUCCESS)
        fail("Authentifizierungsfehler: " + error());
    sftp_ = sftp_new(ssh_);
    if (!sftp_)
        fail("Fehler beim Erstellen der SFTP Session.");
    if (sftp_init(sftp_) != SSH_OK)
        fail("Fehler beim Initialisieren von SFTP.");
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
