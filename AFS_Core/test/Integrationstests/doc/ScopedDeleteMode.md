# ScopedDeleteMode

Module comment for ScopedDeleteMode.h.

## ScopedDeleteMode

The delete mode is global configuration and other test suites change it, so a test that needs
a particular mode sets it itself and restores the previous value - otherwise the recycle bin
would keep a copy and the test order would decide where a deleted file ends up. Shared by
TwoWaySyncTests and SyncFlagTests.
