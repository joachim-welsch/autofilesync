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
