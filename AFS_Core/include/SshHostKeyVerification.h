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

// Verifies the server's host key against the known_hosts store (trust-on-first-use),
// rejecting the connection when a previously stored key no longer matches to guard
// against man-in-the-middle attacks. Call once right after the SSH handshake, before
// authenticating.
#ifdef _UNIX
bool verify_ssh_host_key(ssh_session session, const std::string& host);
#else
    #ifdef _WIN32
bool verify_ssh_host_key(LIBSSH2_SESSION* session, const std::string& host, unsigned short port = 22);
    #endif
#endif

} // namespace AFS_Core
