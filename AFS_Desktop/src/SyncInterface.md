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

## stop-leaves-continuous-mode

Stop ends the continuous mode here and not on the next timer tick: stop_sync() returns at
once when no run is active - within the first second after Start, or with an empty
configuration - and the tick would then find "Continuous" without an instance and start
the next run, or, without sync items, show the "Achtung" box again every second. Found by
GuiTest.StartOhneSynchronisationZeigtHinweis.

## reset-before-the-next-run

The state is reset before the finished handlers run, because countFilesFinished() starts
the copy run right away: resetting afterwards put the controller back to "Waiting" while
that run was active, and request_stop() - the progress dialog's Cancel - then returned
without doing anything, so Cancel let every "Einmal synchronisieren" run through to the
end. Found by GuiTest.AbbrechenImFortschrittsdialogBeendetDenLauf.

## stateText

One line for the status bar saying what the synchronisation is doing right now, refreshed by
the main window on every timer tick: stopped, counting, a single run, a running pass, or
paused with the seconds until the next pass. The pause is the state a user wonders about
most - nothing visibly happens - so it is spelled out instead of shown as "running".

## remainingPause

The continuous run skips every item whose pause has not elapsed, so the run is paused only
while all enabled items are; one due item means work is about to happen. The shortest
remaining pause is what the user waits for. A fresh item's last_sync is TimePoint::min, which
makes it due at once.
