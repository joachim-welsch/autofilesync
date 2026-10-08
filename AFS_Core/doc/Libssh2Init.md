# Libssh2Init

Module comment for Libssh2Init.h / Libssh2Init.cpp.

## ensure_libssh2_initialized

Initialises libssh2's global crypto backend exactly once for the whole process.
libssh2_init()/libssh2_exit() are not reference-counted, so a libssh2_exit() while another
thread still owns a session would tear down the shared state under it; nothing else may call
either of them.
