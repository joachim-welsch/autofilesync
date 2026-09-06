#include "SshConnectTimeout.h"

namespace AFS_Core {

namespace {
    constexpr long kSshConnectTimeoutSeconds = 15;
}

#ifdef _UNIX

void applySshConnectTimeout(ssh_session session)
{
    long timeoutSeconds = kSshConnectTimeoutSeconds;
    ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeoutSeconds);
}

#else
    #ifdef _WIN32

void applySshConnectTimeout(LIBSSH2_SESSION* session)
{
    libssh2_session_set_timeout(session, kSshConnectTimeoutSeconds * 1000L);
}

    #endif
#endif

} // namespace AFS_Core
