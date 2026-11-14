# RestoreDialog

Module comment for RestoreDialog.h / RestoreDialog.cpp.

## RestoreDialog

Shows in one list what the synchronisation deleted into the recycle bin and which earlier
versions it kept, and gives the selected files back (Restore.md#restore). One list instead of
two tabs, because the user looks for a file, not for the mechanism that saved it; the column
"Was" tells the two apart. Each row names the folder the file will be put into, so nothing
happens at a place the user has not read.

## search-thread

The search reads the bin and whole target trees, which takes seconds on a network drive, so it
runs beside the dialog and hands its result over with CallAfter. The items are copied on the
GUI thread before it starts. The destructor asks it to stop and waits for it; a result that is
ready after that is not delivered, and wxWidgets drops a call already queued for a destroyed
window.

## virtual-list

The bin of a long-used installation can hold many thousands of files. A virtual list asks only
for the rows on screen, so opening and filtering stay quick. The list can ask for a row at any
time, also while the rows are being exchanged, so cell_text() answers a row it no longer has
with an empty text, and the rows are emptied before the files behind them change.

## filter

`shown_` holds the indices into `found_` that pass the filter, in the order of `found_` (newest
first). The filter looks at the file name and the folder, ignoring case. The selection is
cleared whenever the rows change, because a virtual list keeps selected row numbers, not files.

## put_back

"Wiederherstellen" puts every selected file into the folder its row names. "Wiederherstellen
nach…" asks for one folder and puts all of them there. A file whose folder is unknown
(Restore.md#place_known) makes the first button ask as well. Nothing is ever replaced
(Restore.md#free_name), so no confirmation is needed. Deleted files leave the list once they
are back; earlier versions stay, since the version itself remains in the target.

## status

The line under the list says what happened and where the file lies now, including the new name
when the original one was taken. It replaces a message box per restore: the user stays in the
list and can go on. Only failures get a box, because they must not be overlooked; the reason
is in the log. The text is wrapped to a fixed width because a wxStaticText does not wrap by
itself.
