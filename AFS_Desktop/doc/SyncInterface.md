# SyncInterface

Module comment for SyncInterface.h / SyncInterface.cpp.

## terminal-outcome-reenables-menu

This is a terminal outcome (no start_sync()/sync_once_finished() follows to do it): re-enable what
on_start_single_sync() disabled, or the menu stays greyed out forever whenever a sync legitimately
finds nothing to do.

## console-single-sync

Bypasses the GUI's count-then-sync two-phase dance (built for a progress dialog's file-count
range) and start_sync()'s unconditional wxMessageBox for count_files_ == 0 - both would otherwise
leave a --console invocation (a cron/systemd job with nobody watching) stuck on a modal dialog
it can never dismiss, without ever having synced anything.

## poll-for-completion

Poll for actual completion instead of calling stop_sync(): that requests an early stop, aborting
mid-item and syncing only whatever the worker thread happened to reach in the few CPU cycles
before this call.

## stop-leaves-continuous-mode

Stop ends the continuous mode here and not on the next timer tick: stop_sync() returns at
once when no run is active - within the first second after Start, or with an empty
configuration - and the tick would then find "Continuous" without an instance and start
the next run, or, without sync items, show the "Achtung" box again every second. Found by
GuiTest.StartOhneSynchronisationZeigtHinweis.

## reset-before-the-next-run

The state is reset before the finished handlers run, because count_files_finished() starts
the copy run right away: resetting afterwards put the controller back to "Waiting" while
that run was active, and request_stop() - the progress dialog's Cancel - then returned
without doing anything, so Cancel let every "Einmal synchronisieren" run through to the
end. Found by GuiTest.AbbrechenImFortschrittsdialogBeendetDenLauf.

## counting_cursor_

The count pass runs on the worker thread and ends on a later timer tick, but the scoped
wxBusyCursor that start_sync() used to create ended with the call: over a large tree the user
saw a normal cursor and a greyed-out menu for seconds with nothing to tell them the program
was working. The cursor now lives in a member for the whole pass and is released in
count_files_finished() before the "Info" box or the progress dialog appears, and in
start_sync() when the pass could not be started.

## state_text

One line for the status bar saying what the synchronisation is doing right now, refreshed by
the main window on every timer tick: stopped, counting, a single run, a running pass, or
paused with the seconds until the next pass. The pause is the state a user wonders about
most - nothing visibly happens - so it is spelled out instead of shown as "running". The state
starts as None: it used to start as Single, which nothing noticed while the status bar did not
show it, and which made a freshly started program claim a single run was in progress until the
first real one ended.

## remaining_pause

The continuous run skips every item whose pause has not elapsed, so the run is paused only
while all enabled items are; one due item means work is about to happen. The shortest
remaining pause is what the user waits for. A fresh item's last_sync is TimePoint::min, which
makes it due at once.

## show_run_messages

"Einmal synchronisieren" is the run a user watches, so its messages are collected
(Logging.md#collected_) from the count pass to the end and shown once, in one box, filtered by
the Fehler/Warnung/Info settings - one box per message would bury the user under the infos of
a large run, and a box from the worker thread is not possible anyway. The continuous run and
the silent modes (console, start minimised) show nothing; the log has it all. The box also
follows the "nothing found" case, because an unreachable source ends there.

## run_messages_text

Each line carries its level, because the box mixes levels when several filters are on, and the
box is capped at MAX_LINES_IN_BOX lines: a message box is not a log viewer, and the log has the
rest.

## preview

"Vorschau…" runs a SyncType::Preview pass (Synchronize.md#preview) the way "Einmal
synchronisieren" runs its counting pass: busy cursor, start commands disabled, messages
collected, finished on a timer tick. preview_finished() then takes the plan from the
controller and shows the PreviewDialog; "Jetzt synchronisieren" starts the ordinary single
run from there, so what follows a preview is exactly what the menu item would do. An empty
plan ends in a box instead of an empty dialog, and the messages of the pass - an unreachable
source is the usual one - follow in a second box, as they would after a single run.

## enable_start_commands

Start, "Einmal synchronisieren" and "Vorschau…" are enabled and disabled together: all
three start a pass, and none of them may start while one runs. One function instead of three
Enable() calls at every place that toggles them, because the preview was forgotten at one
such place during its introduction and found by the tests.

## show_state

Writes state_text() into the status bar's second field, the one MainWindow reserves for the
synchronisation (MainWindow.md#update_sync_state). The main window refreshes it on its timer
tick, but a box or dialog shown from a finished handler runs its own event loop before that
tick comes, so the bar kept saying "Vorschau wird ermittelt …" or "Zähle Dateien …" the whole
time the result was on screen; found with the first screenshot of the preview. The handlers
therefore show the new state themselves before they open anything.
