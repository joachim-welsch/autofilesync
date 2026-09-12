# SyncInterface

Module comment for SyncInterface.h / SyncInterface.cpp.

## terminal-outcome-reenables-menu

This is a terminal outcome (no startSync()/syncOnceFinished() follows to do it): re-enable what
onStartSingleSync() disabled, or the menu stays greyed out forever whenever a sync legitimately
finds nothing to do.

## console-single-sync

Bypasses the GUI's count-then-sync two-phase dance (built for a progress dialog's file-count
range) and startSync()'s unconditional wxMessageBox for countFiles_ == 0 - both would otherwise
leave a --console invocation (a cron/systemd job with nobody watching) stuck on a modal dialog
it can never dismiss, without ever having synced anything.

## poll-for-completion

Poll for actual completion instead of calling stopSync(): that requests an early stop, aborting
mid-item and syncing only whatever the worker thread happened to reach in the few CPU cycles
before this call.
