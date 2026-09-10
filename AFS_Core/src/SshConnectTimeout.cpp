#include "SshConnectTimeout.h"

namespace AFS_Core {

namespace {
    constexpr long SSH_CONNECT_TIMEOUT_SECONDS = 15;
}

#ifdef _UNIX

void apply_ssh_connect_timeout(ssh_session session)
{
    long timeout_seconds = SSH_CONNECT_TIMEOUT_SECONDS;
    ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout_seconds);
}

#else
    #ifdef _WIN32

void apply_ssh_connect_timeout(LIBSSH2_SESSION* session)
{
    libssh2_session_set_timeout(session, SSH_CONNECT_TIMEOUT_SECONDS * 1000L);
}

    #endif
#endif

} // namespace AFS_Core
