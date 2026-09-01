#pragma once

#ifdef _WIN32

#include <mutex>
#include <string>
#include <libssh2.h>
#include "Logging.h"

// Ensures libssh2's global crypto backend is initialized exactly once for
// the whole process. libssh2_init()/libssh2_exit() are not reference-counted,
// so calling libssh2_exit() while any other thread still owns a session tears
// down the shared backend state from under it. Callers must not call
// libssh2_init()/libssh2_exit() themselves.
inline void ensureLibssh2Initialized()
{
    // Implementation is inline so translation units that include this header
    // get the initialization logic without requiring an extra .cpp to be
    // added to the project. Use std::call_once to make this thread-safe.
    static std::once_flag onceFlag;
    std::call_once(onceFlag, []() {
        int rc = libssh2_init(0);
        if (rc != 0) {
            // Logging may not be available very early during startup, but use
            // the project's Logging facility if possible.
            try {
                Logging::instance().log(LogLevel::Error, std::string("libssh2_init failed: ") + std::to_string(rc));
            } catch (...) {
                // swallow any logging errors - initialization failure is
                // reported via return code only.
            }
        }
    });
}

#endif
