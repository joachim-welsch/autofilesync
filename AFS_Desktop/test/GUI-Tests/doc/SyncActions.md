# SyncActions

Module comment for SyncActions.h / SyncActions.cpp.

## SyncActions

Starting a synchronisation the way a user does and waiting for its end, so a test can look at
the file system afterwards. The menu commands are the user's only way to start one, and the
test observes the same state the menu's enable rules and the progress dialog hang on.

## add_sync_item

The way every sync test begins: the synchronisation is created through the dialog, not
written into the configuration, so a test of the run also covers that the dialog delivers
the options it was given.

## wait_until_sync_finished

A run ends in two steps: the worker thread flags completion, and the main window's 1 s timer
then moves SyncInterface back to SyncType::None and destroys the Synchronize instance. Both
are waited for: after the first the files may be there, but the menu is still locked and a
second start would be refused. Pumping inside wait_until() is what lets the timer fire.

## sync_once

"Einmal synchronisieren" first counts the files, then copies them - two runs back to back,
which is why the wait watches the sync type and not a single run. A configuration without
anything to copy ends in the "Info" message box, which the test has to announce through
ModalDialogWatcher::answer() beforehand.

## progress_dialog

The progress dialog is modeless (Show(), not ShowModal()), so the modal hook never sees it;
it is found among the top-level windows instead. Only a shown one counts: SyncInterface
destroys it with Destroy(), which defers the deletion, so a hidden one may linger for a
moment after the run.

## current_file_shown

The dialog exposes no accessor for its file label, so the label is read through the dialog's
only wxStaticText, and the bar through its only wxGauge: a user recognises the file by its
name in that label, and the bar is what tells them how far the run is.

## watch_progress_dialog

Samples the shown progress dialog every 10 ms until it is gone: every bar value, and the file
label whenever it changes. A single look at the dialog could fall before the first timer tick
or after the last one, so the tests judge the whole course instead - a bar that moves, rises
monotonically and ends full, and a label that names the files one after the other. Returns
nothing when the dialog is still open at the timeout.
