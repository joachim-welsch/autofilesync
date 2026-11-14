# RestoreDialogTest

Module comment for RestoreDialogTest.cpp.

## RestoreDialogTest

Every test gets an empty recycle bin inside the sandbox and the delete mode set to the bin;
RestoredSettings puts both back. Files are deleted through delete_file(), the way a
synchronisation deletes them, so the dialog reads a bin it would also meet in real use. The
bin is removed again before the fixture compares the state.

## in_restore_dialog

Opens "Synchronisation → Dateien wiederherstellen…", waits until the dialog's search has
delivered its result, runs the test's actions and closes the dialog. The search runs on a
thread of its own (RestoreDialog.md#search-thread), which is why the test waits for the status
line to change instead of reading the list at once.

## status_of

The status line is wrapped to the dialog's width, which puts line breaks into the label; they
are turned back into spaces so the text can be compared with the translation.

## FruehereFassung

The synchronisation is only added, never run: the earlier version is laid into the target by
the test. What counts is that the dialog finds it through the item's father-son option and
copies it into the source, beside the current file, leaving both that file and the version in
the target as they were.
