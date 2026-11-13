# SynchronizeTests

Module comment for SynchronizeTests.cpp.

## stop-flag-regression

Regression test for Fehler 7: the worker thread used to start before stop_ was reset to false,
so a thread that reached its "while (!stop_)" check before create() got to run could see the
stale "true" left over from program start and silently exit after a single pass instead of
looping.

## second-pass-timing

cv_.wait_for() inside the loop times out after 1s; waiting past that guarantees a second pass
has started if the loop is still alive.

## OneItemFilesystemErrorDoesNotStopOtherItems

Regression test for Fehler 8: CheckPath uses the throwing fs:: overloads, so a single item whose
path is inaccessible (e.g. permission denied) used to throw out of Synchronize::sync_item()
uncaught, aborting the whole sync pass - including every other configured item - instead of just
skipping that one.

## second-item-reached

The second item's last_sync() only updates if sync_item() actually reached it, i.e. the
exception from the first item did not abort the loop.

## SshTargetDirCheckIsCachedAcrossPasses

Fehler 10 (round 4): every sync pass called ssh_is_directory() for a File item with an SSH
target - a complete SSH connect/handshake/auth/teardown per pass, hammering the server once a
second in continuous sync. The result is now cached per target with a TTL in
Synchronize::is_ssh_target_dir(), so repeated passes must not reconnect for this check.

## needs-reachable-server

An empty host used to be enough here, because CheckPath let every SSH path through unchecked and
the pass still got as far as the target directory check. Now that CheckPath really probes the
remote side, an unreachable target pauses the item before that - so the test needs a reachable
server.

## preconditions-for-check

An existing source file and an existing target directory: only then does the pass get as far as
the check this test is about.

## account-before-syncitem

Account must exist before the SyncItem is built, or the target does not parse as an SshPath.

## request-stop-must-not-block

Fehler 6 (round 7): ProgressDialog::OnCancel() called the blocking SyncControler::stop_sync()
directly on the GUI thread - its std::this_thread::sleep_for()-based wait loop doesn't pump the
event queue, freezing the whole window for however long the worker took to notice the stop
request. The new request_stop() only signals and must return immediately, leaving the actual
(now GUI-responsive) waiting to the caller.

## cleanup-via-destroy

Cleanup via destroy_synchronize() (its ~Synchronize() signals stop_ and joins the worker thread
directly), not sc.sync_finished(): state_ only transitions to SyncFinished under #ifdef
_WXWIDGETS - not compiled into this wx-less test binary - so waiting on it here would hang
forever.

## NewerTargetIsLoggedExactlyOnceAcrossPasses

Runs the real continuous loop over an item whose target is one hour newer than
its source and that neither copies newer targets nor syncs both ways. After at
least three passes the log must hold the "target is newer" info exactly once,
and the target must be untouched. The paths carry the pid, so nothing an earlier
test made SyncFile remember can suppress the message.

## ContentConflictIsLoggedExactlyOnceAcrossPasses

Runs the real continuous loop over a two-way item whose files have the same time
and size but different content. After at least three passes the log must hold
the conflict exactly once, at level Error, and both files must be untouched.

## LastRunIsRecordedInTheConfiguration

The assistant reads the outcome of each item from the configuration, so a real pass of the
engine must leave it there - on the live item, not on the snapshot the pass worked on.

## SyncScheduleTest

Which items a pass runs at all: Synchronize::sync_item() skips a disabled item and, except in a
preview, an item whose pause since its last run has not elapsed. Both decisions had no test;
every other test sets sync_cycle to 0 or a fresh item precisely so that the pause never gets in
the way. The fixture owns one Dir item whose target starts empty, so "did the item run" is
simply "is a.txt in the target", and destroys a leftover Synchronize in TearDown because a
failed assertion must not leave the worker thread running into the next test.

## pass

create() starts the worker thread; the sleep gives one pass time to finish before the test
looks (400 ms, like the other tests here). The caller destroys the instance when it is done
with it, or lets TearDown do it.

## DisabledItemIsSkippedByARun

Skipped means untouched in every respect: no copy, no last_sync, no last_run - the summary
and the assistant read those and must not report a run that never happened.

## cycle-timing

The continuous loop wakes every second (Synchronize::operator()), so 2.5 s after the first pass
at least two further passes have happened: with a 30 s cycle none of them may run the item
(b.txt stays out), with a 1 s cycle one of them must. The pause survives the end of a run
because last_sync lives in the configuration's item, which is why a second single run also
waits.

## PreviewIgnoresThePause

Synchronize.md#preview-ignores-the-pause: right after a run the preview still has to look at
the item and plan b.txt, while copying nothing. The plan is read through the _TEST accessor, as
unreachable() is.

## watched-run

A continuous run with a pause of 60 s: a file written after the first pass reaches the target
within a few seconds when the item is watched (Synchronize.md#due: one second of loop, two of
settling) and not within four seconds when it is not, which is the old behaviour the option
leaves untouched. The fixture resets the configuration and removes its directories, and
destroys the Synchronize first so that no pass runs into the removal.

## AChangeDuringAPassOfAWatchedItemIsNotLost

The 64 MiB file keeps the first pass busy: the target file exists from the first block on, and
the change is written as soon as it appears. last_sync is set only at the end of a pass, so a
last_sync still at its start value proves that the change fell into the pass. The pass listed
its directory before, so only the next pass can copy the new file (Synchronize.md#due). A machine fast enough to copy 64 MiB before the check needs a larger
file, which the assertion message names.

## comes_to_rest

A pass sets the item's last_sync, so a last_sync that stays the same for five seconds -
longer than the settle time plus the loop's tick - means no pass followed. The passes after a
change (the one that copies, the one its own writes cause) must end there; an endless chain
would keep last_sync moving and run into the timeout.
