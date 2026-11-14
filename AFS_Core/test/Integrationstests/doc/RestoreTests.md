# RestoreTests

Module comment for RestoreTests.cpp.

## RestoreTest

Each test has a directory of its own below the temp directory with a source and a target, a
recycle bin of its own (ScopedRecycleBin) and the delete mode set to the bin. Files are deleted
through delete_file(), so the tests read the bin layout the synchronisation really writes.

## DeletedFileOfATargetBelongsIntoTheSource

The central case: a mirroring item deleted a file from the target, and the user wants it back.
It has to appear in the source, with its content, and must not be offered a second time.

## ExistingFileIsNeverOverwritten

Two deleted versions of one file are put back while a third exists under that name. All three
have to survive. Both restorables are given the same time to force the counter after the date.

## FilesOfADeletedDirectoryKeepTheirPlace

A directory put into the bin as a whole carries the timestamp on its own name, not on its files.

## EarlierVersionIsCopiedBesideTheCurrentFile

The version is made two days old so its time can be told from the time of the restore: the
copy has to be new, the version itself has to stay where it is and keep being offered.
