# FileUtil

Module comment for FileUtil.h / FileUtil.cpp.

## path_contains

Windows hands back '\' and possibly a different capitalisation.

## write_text_file

Creates a test file in one line, including its parent directories, and optionally backdates it.
Which of two files is the newer one is the input of almost every sync decision, so the tests need
an mtime they control rather than "both written in the same millisecond" - the age is subtracted
from the current time because only the relative order matters.

## write_large_file

A file of the given size, written in 1 MiB blocks of the same letter. The cancel tests need a
copy that takes long enough for a second thread to notice it has begun and to call
Synchronize::stop() while blocks are still pending - a few bytes are copied before any thread
gets scheduled.

## test-dirs-follow-the-temp-dir

Below the temp directory of the process rather than a fixed /tmp: TestMain.cpp
gives each run a temp directory of its own, watches it for leftovers and removes
it at the end. The SSH tests reach these directories through the local SSH
server as another user, which is why the tests open up the permissions of the
directories they share with it.
