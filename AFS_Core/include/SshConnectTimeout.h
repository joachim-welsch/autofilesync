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
// operation (ssh_list_dir(), ssh_exists(), copy_ssh_to_fs() and friends) opens its
// own short-lived connection rather than reusing one across a sync pass, so
// without this a single unresponsive remote host - asleep or dropped off the
// network mid-handshake - blocks the sync worker thread forever: nothing
// checks Synchronize::stopping() inside a blocking network call, and the
// GUI's Cancel button just waits on that same thread to finish.
#ifdef _UNIX
void apply_ssh_connect_timeout(ssh_session session);
#else
    #ifdef _WIN32
// Covers the handshake and later blocking libssh2 I/O, not the raw socket
// connect() that precedes libssh2_session_init() - that one still has no
// timeout of its own.
void apply_ssh_connect_timeout(LIBSSH2_SESSION* session);
    #endif
#endif

} // namespace AFS_Core
