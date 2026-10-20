# SyncItemDialogRulesTest

Module comment for SyncItemDialogRulesTest.cpp.

## inspect_and_cancel

These tests are about the dialog's own rules, not about creating anything: the action reads
and pokes the controls and always cancels, so the configuration is untouched and the fixture's
TearDown has nothing to find.

## ok_enabled

OK is guarded by EVT_UPDATE_UI, which only runs while the event loop is idle; the pump before
reading the state is what lets the rule catch up with the last keystroke.

## option-locks

Options lock each other (SyncItemDialog.md#option-locks): a move locks two-way sync and
"Gelöschte Dateien im Ziel löschen" and is locked by either of them; two-way sync and
"Kopieren wenn Ziel neuer" lock each other; the latter is locked without a time or size
criterion; the type Datei locks "Gelöschte Dateien im Ziel löschen". Every lock is tested in
both directions - locking and releasing - because a lock that never releases is a bug the user
cannot work around, and where a lock clears the option, the cleared value is checked too.

## exclusions-round-trip

The saving direction is covered by SyncOptionsTest; this test opens the edit dialog for an
item that already has exclusions and reads them back from the text box, one per line. The
item goes straight into the configuration: the round trip through the dialog is what is
under test, not the creation.

## two-way-implies-moves

Two-way sync ticks and locks "Umbenennen und Verschieben erkennen" and shows the hint line
under the options (SyncItemDialog.md#track-moves-implied, #two-way-hint); unticking it frees
the option and hides the hint again. The type Datei locks the option on its own, since a file
item has nothing to rename.
