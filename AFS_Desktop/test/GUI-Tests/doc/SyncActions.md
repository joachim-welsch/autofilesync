# SyncActions

Module comment for SyncActions.h / SyncActions.cpp.

## SyncActions

Starting a synchronisation the way a user does and waiting for its end, so a test can look at
the file system afterwards. The menu commands are the user's only way to start one, and the
test observes the same state the menu's enable rules and the progress dialog hang on.

## addSyncItem

The way every sync test begins: the synchronisation is created through the dialog, not
written into the configuration, so a test of the run also covers that the dialog delivers
the options it was given.

## waitUntilSyncFinished

A run ends in two steps: the worker thread flags completion, and the main window's 1 s timer
then moves SyncInterface back to SyncType::None and destroys the Synchronize instance. Both
are waited for: after the first the files may be there, but the menu is still locked and a
second start would be refused. Pumping inside waitUntil() is what lets the timer fire.

## syncOnce

"Einmal synchronisieren" first counts the files, then copies them - two runs back to back,
which is why the wait watches the sync type and not a single run. A configuration without
anything to copy ends in the "Info" message box, which the test has to announce through
ModalDialogWatcher::answer() beforehand.

## progressDialog

The progress dialog is modeless (Show(), not ShowModal()), so the modal hook never sees it;
it is found among the top-level windows instead. Only a shown one counts: SyncInterface
destroys it with Destroy(), which defers the deletion, so a hidden one may linger for a
moment after the run.
