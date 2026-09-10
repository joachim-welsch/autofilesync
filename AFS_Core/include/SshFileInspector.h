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
  // Connects, verifies the host key and logs in; throws if any of that fails,
  // so a constructed object means the remote side is up.
  SshFileInspector(const SshAccount& account);
  ~SshFileInspector();
  // remote_path is the path on the remote side without the account prefix
  // ("/home/user/data", not "account:/home/user/data"); Windows paths are
  // transcribed here ("C:\dir" -> "/C:/dir"). exists=false means "connected
  // and definitely absent"; any other failure throws so that a connection
  // problem does not look like a deleted file.
  RemoteFileInfo inspect_file(const std::string& remote_path);
};

} // namespace AFS_Core
