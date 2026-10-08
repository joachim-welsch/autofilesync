# SyncItemDialogRulesTest

Module comment for SyncItemDialogRulesTest.cpp.

## inspectAndCancel

These tests are about the dialog's own rules, not about creating anything: the action reads
and pokes the controls and always cancels, so the configuration is untouched and the fixture's
TearDown has nothing to find.

## okEnabled

OK is guarded by EVT_UPDATE_UI, which only runs while the event loop is idle; the pump before
reading the state is what lets the rule catch up with the last keystroke.

## option-locks

Two options are locked by others: two-way sync clears and disables "Kopieren wenn Ziel
neuer", and the type Datei clears and disables "Gelöschte Dateien im Ziel löschen". Both
directions are tested - locking and releasing - because a lock that never releases is a bug
the user cannot work around.

## exclusions-round-trip

The saving direction is covered by SyncOptionsTest; this test opens the edit dialog for an
item that already has exclusions and reads them back from the text box, one per line. The
item goes straight into the configuration: the round trip through the dialog is what is
under test, not the creation.
