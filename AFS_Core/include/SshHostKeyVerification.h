#pragma once

#include <string>

#ifdef _UNIX
    #include <libssh/libssh.h>
#else
    #ifdef _WIN32
        #include <libssh2.h>
    #endif
#endif

namespace AFS_Core {

#ifdef _UNIX
bool verify_ssh_host_key(ssh_session session, const std::string& host);  // [SshHostKeyVerification.md#verify_ssh_host_key]
#else
    #ifdef _WIN32
bool verify_ssh_host_key(LIBSSH2_SESSION* session, const std::string& host, unsigned short port = 22);
    #endif
#endif

} // namespace AFS_Core
