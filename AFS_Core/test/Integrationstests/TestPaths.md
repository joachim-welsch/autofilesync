# TestPaths

Module comment for TestPaths.h.

## TestPaths

Test fixtures need two kinds of on-disk locations: read-only resources checked into the repo,
and a writable scratch area for sync source/target dirs. Both used to be hardcoded to one
developer's Linux home directory, which only exists on that one machine.
LIB_SOURCE_DIR (set from $HOME by CMake) and the OS temp dir make the fixtures work on any
machine and platform.
