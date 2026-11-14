# FileDeleteTests

Module comment for FileDeleteTests.cpp.

## RecycleBinModeMirrorsTheOriginDirectory

The bin mirrors the origin below itself, so a restore can tell where the file came from - and
two files of the same name from different directories do not land on top of each other.

## RecycleBinModeKeepsEveryDeletionOfTheSameFile

The same file is written and deleted two hundred times in a row, far faster than one deletion
per millisecond, so several deletions fall into the same millisecond. Each must leave an item
of its own in the bin; with the timestamp alone as the name, later ones replaced earlier ones.
