# Libssh2InitTests

Module comment for Libssh2InitTests.cpp.

## OnlyLibssh2InitCppCallsLibssh2InitOrExit

Regression guard for Fehler 3: libssh2_init()/libssh2_exit() are process-global and not
reference-counted, so calling them per SSH session/object races with any other thread still
using libssh2. Only Libssh2Init.cpp may call them directly; every other translation unit must go
through ensure_libssh2_initialized().

## EnsureLibssh2InitializedIsIdempotent

Repeated calls must not re-run libssh2_init(), which would corrupt the shared crypto backend
state out from under any session still using it; libssh2 must stay usable afterward.

## EnsureLibssh2InitializedIsThreadSafe

Simulates the GUI thread and the sync worker thread racing to initialize libssh2 at the same
time - exactly the scenario Fehler 3 broke.
