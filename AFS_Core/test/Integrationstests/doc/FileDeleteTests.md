# FileDeleteTests

Module comment for FileDeleteTests.cpp.

## RecycleBinModeMirrorsTheOriginDirectory

The bin mirrors the origin below itself, so a restore can tell where the file came from - and
two files of the same name from different directories do not land on top of each other.
