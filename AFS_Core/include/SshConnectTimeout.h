#pragma once

#ifdef _UNIX
    #include <libssh/libssh.h>
#else
    #ifdef _WIN32
        #include <libssh2.h>
    #endif
#endif

namespace AFS_Core {

// Bounds how long an SSH connection attempt may block. Every "account:/path"
// operation (sshListDir(), sshExists(), copySshToFs() and friends) opens its
// own short-lived connection rather than reusing one across a sync pass, so
// without this a single unresponsive remote host - asleep or dropped off the
// network mid-handshake - blocks the sync worker thread forever: nothing
// checks Synchronize::stopRequested() inside a blocking network call, and the
// GUI's Cancel button just waits on that same thread to finish.
#ifdef _UNIX
void applySshConnectTimeout(ssh_session session);
#else
    #ifdef _WIN32
// Covers the handshake and later blocking libssh2 I/O, not the raw socket
// connect() that precedes libssh2_session_init() - that one still has no
// timeout of its own.
void applySshConnectTimeout(LIBSSH2_SESSION* session);
    #endif
#endif

} // namespace AFS_Core
