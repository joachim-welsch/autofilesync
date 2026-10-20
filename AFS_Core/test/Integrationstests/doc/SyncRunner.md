# SyncRunner

Module comment for SyncRunner.h.

## run_sync

Runs one synchronisation pass for a SyncItem of any type and returns the number of copied
files. Header-only because SyncTree inherits SyncBase privately, so the three sync classes have
no common base a test could dispatch through - the switch is the only way, and no test should
repeat it.

## run_sync-with-state

The same pass with the sync state of a two-way item (SyncBase.md#state_), as Synchronize hands
it down - and nothing more: the pruning and the file belong to Synchronize's keeper, which
SyncStateRunTests covers, so a test here sees exactly what the pass recorded.
