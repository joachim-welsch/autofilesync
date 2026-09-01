#include "pch.h"
#include "Libssh2Init.h"

#ifdef _WIN32
#include <libssh2.h>

namespace {

// Constructed on first use (thread-safe per C++11 static local semantics) and
// destroyed once at process exit, giving the whole process a single
// init/exit pair regardless of how many SSH sessions or threads use libssh2.
struct Libssh2Guard {
    Libssh2Guard() { libssh2_init(0); }
    ~Libssh2Guard() { libssh2_exit(); }
};

} // namespace

//void ensureLibssh2Initialized()
//{
//    static Libssh2Guard guard;
//}

#endif
