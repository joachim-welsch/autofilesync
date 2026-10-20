# SyncOptionsTest

Module comment for SyncOptionsTest.cpp.

## EmptiedRecycleBin

The sandbox redirects the home directory, so the recycle bin is the sandbox's own
`.recyclebin` and a deleted file lands there instead of anywhere real. The test reads the
item count as proof of the delete mode and empties the bin when it leaves, so the next
delete test starts with an empty one. It empties the bin only when the path is inside the
sandbox and fails otherwise: an earlier version removed the developer's real recycle bin
when the sandbox did not yet redirect HOMEDRIVE/HOMEPATH (GuiTestSandbox.md#homedrive-homepath).

## recycle-bin-tests

"Quelle nach Kopieren löschen" and "Gelöschte Dateien im Ziel löschen" are the two options
that remove data, and both are proven twice: the file is gone from where it was and it is in
the recycle bin, i.e. removed according to the delete mode, not lost.

## GenerationenprinzipLegtBakDateiAn

The second run has to see a newer source, and the comparison is by date with a tolerance of
two seconds: ageing the source by an hour before the first run puts the rewrite far outside
that tolerance, whatever the file system's timestamp resolution. The pause is for the item's
cycle of one second - a single run skips an item that was synchronised less than a cycle ago.

## write_time_apart

Puts the source one second ahead of the target: within the default tolerance, so the pair is
equal to the date comparison and the different content stays where it is. The second sandbox
sets the tolerance to 0 in the dialog and proves that the same second then counts as a change.

## same_write_time

Same size, same write time, different content: the case only a content comparison can tell
apart. The pair of tests shows both sides - the content comparison copies, the date
comparison reports nothing to do.

## two-way-state

The sync state through the GUI: the second single run of a two-way item carries a deletion in
the source to the target (SyncFile.md#state), into the recycle bin, and a one-way item with
"Umbenennen und Verschieben erkennen" renames in the target - which the empty recycle bin
proves, since a copy-and-delete would have put the old file there. Both items leave a state
file the fixture removes afterwards (GuiTest.md#sync-state-removed).

## mass-deletion

The valve from the user's side (SyncBase.md#mass-deletion, SyncInterface.md#preview-approves-deletions):
"Einmal synchronisieren" over an emptied target refuses and explains itself in the error box
after the run, source and recycle bin untouched; the preview lists the twelve deletions and
"Jetzt synchronisieren" there carries them out, into the recycle bin.

## let_the_pause_elapse

A single run skips an item whose pause since its last run has not elapsed
(Synchronize.md#preview-ignores-the-pause names the one exception), and the dialog's default
pause is one second. A second "Einmal synchronisieren" right after the first therefore counts
nothing, answers with the "no files" box and never runs - which looks exactly like a refused
deletion. Every test that runs twice pumps events for a little over a second in between.
