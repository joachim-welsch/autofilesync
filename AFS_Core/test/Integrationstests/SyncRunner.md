# SyncRunner

Module comment for SyncRunner.h.

## run_sync

Runs one synchronisation pass for a SyncItem of any type and returns the number of copied
files. Header-only because SyncTree inherits SyncBase privately, so the three sync classes have
no common base a test could dispatch through - the switch is the only way, and no test should
repeat it.
