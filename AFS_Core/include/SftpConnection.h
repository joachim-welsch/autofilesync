#pragma once
#ifdef _UNIX
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <memory>
#include <string>
#include "SshAccount.h"

namespace AFS_Core {

using SftpFile = std::unique_ptr<sftp_file_struct, int(*)(sftp_file)>;
using SftpDir = std::unique_ptr<sftp_dir_struct, int(*)(sftp_dir)>;
using SftpAttributes = std::unique_ptr<sftp_attributes_struct, void(*)(sftp_attributes)>;

// Owns one libssh session with its SFTP channel; both are released in the destructor
class SftpConnection {
    ssh_session ssh_ = nullptr;
    sftp_session sftp_ = nullptr;
    void release();
    [[noreturn]] void fail(const std::string& message);
public:
    SftpConnection() = default;
    explicit SftpConnection(const SshAccount& account);
    SftpConnection(const SftpConnection&) = delete;
    SftpConnection& operator=(const SftpConnection&) = delete;
    ~SftpConnection();
    // Connects, verifies the host key and logs in; throws std::runtime_error when a step fails
    void connect(const SshAccount& account);
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
