#pragma once

namespace AFS_Core {

#ifdef _WIN32
// Initialises libssh2's global crypto backend exactly once for the whole
// process. libssh2_init()/libssh2_exit() are not reference-counted, so a
// libssh2_exit() while another thread still owns a session would tear down
// the shared state under it; nothing else may call either of them.
void ensure_libssh2_initialized();
#endif

} // namespace AFS_Core
