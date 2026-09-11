#include "pch.h"
#include "Libssh2Init.h"
#ifdef _WIN32
#include <libssh2.h>
#include <mutex>
#include <string>
#include "Logging.h"

namespace AFS_Core {

void ensure_libssh2_initialized()
{
    static std::once_flag init_once;
    std::call_once(init_once, []() {
        const int rc = libssh2_init(0);
        if (rc != 0)
            log_error(std::string("libssh2_init failed: ") + std::to_string(rc));
    });
}

} // namespace AFS_Core
#endif
