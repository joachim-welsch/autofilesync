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

The second run has to see a newer source, and the comparison is by date: the pause before
the rewrite keeps the two write times apart on a file system with coarse timestamps.

## same_write_time

Same size, same write time, different content: the case only a content comparison can tell
apart. The pair of tests shows both sides - the content comparison copies, the date
comparison reports nothing to do.
