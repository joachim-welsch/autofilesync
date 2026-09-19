#pragma once

#include <string>

#ifdef _UNIX
    #include "SftpConnection.h"
#else
    #ifdef _WIN32
        #include <libssh2.h>
        #include <libssh2_sftp.h>
    #endif
#endif

#include "SshAccount.h"
#include "SshUnreachable.h"

namespace AFS_Core {

struct RemoteFileInfo {
    bool exists;
    uint64_t size_bytes;
    std::string last_modified_iso;
    time_t raw_mtime;
};

class SshFileInspector {
#ifdef _UNIX
    SftpConnection connection_;
#else
    #ifdef _WIN32
        LIBSSH2_SESSION* my_ssh_session_ = nullptr;
        LIBSSH2_SFTP* sftp_session_ptr_ = nullptr;
        SOCKET sock_ = INVALID_SOCKET;
        void cleanup();
    #endif
#endif
    SshAccount account_;
public:
  SshFileInspector(const SshAccount& account);  // [SshFileInspector.md#SshFileInspector]
  ~SshFileInspector();
  RemoteFileInfo inspect_file(const std::string& remote_path);  // [SshFileInspector.md#inspect_file]
};

} // namespace AFS_Core
