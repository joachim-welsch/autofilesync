#pragma once

namespace AFS_Core {

#ifdef _WIN32
void ensure_libssh2_initialized();  // [Libssh2Init.md#ensure_libssh2_initialized]
#endif

} // namespace AFS_Core
