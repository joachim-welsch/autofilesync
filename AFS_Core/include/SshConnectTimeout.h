#pragma once

#ifdef _UNIX
    #include <libssh/libssh.h>
#else
    #ifdef _WIN32
        #include <libssh2.h>
    #endif
#endif

namespace AFS_Core {

#ifdef _UNIX
void apply_ssh_connect_timeout(ssh_session session);  // [SshConnectTimeout.md#apply_ssh_connect_timeout]
#else
    #ifdef _WIN32
void apply_ssh_connect_timeout(LIBSSH2_SESSION* session);  // [SshConnectTimeout.md#windows-timeout-scope]
    #endif
#endif

} // namespace AFS_Core
