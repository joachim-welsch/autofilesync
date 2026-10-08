# RecycleBinTests

Module comment for RecycleBinTests.cpp. FileDeleteTests.cpp covers the way into the bin through
delete_file(); these tests address the bin directly, including the way back.

## RecycleBinTest

Items are relative to the current working directory, so every test runs inside a working
directory of its own. Both the working directory and the path of the bin are process-wide state
and are put back afterwards, otherwise later tests would resolve their relative paths elsewhere.

## RestoreItemPutsTheFileBackWhereItCameFrom

The origin directory is removed first: a directory tree deleted as a whole has to be restorable
file by file.

## RestoreItemNeverOverwritesAnExistingFile

Between deletion and restore the user may have created a new file of the same name. Neither of
the two versions may be lost, so the restore fails and the item stays in the bin.

## items-must-stay-inside-the-bin

An absolute path or a ".." that climbs above the bin would make add_item() move a file to, and
restore_item() take a file from, an arbitrary place of the filesystem. The second add_item() of
ItemsEscapingTheBinAreRejected hides the escape behind a directory name, which only shows after
the path is normalized.

## ListItemsSurvivesANewBinPath

The items are read from the directory and not from a member list, so they are still there after
the bin was pointed elsewhere and back - the same happens at a restart of the program.

## ADanglingSymlinkCanBeBinnedAndRestored

fs::exists() follows the link and answers false for a dangling one. Such a link is still an entry
the synchronisation has to be able to delete, and restoring it must not resolve it either.

## ConcurrentAddsLoseNoItem

The synchronisation deletes from several threads at once; the threads race in creating the
mirrored directories and in moving the files.
