# FileUtil

Module comment for FileUtil.h / FileUtil.cpp.

## path_contains

Windows hands back '\' and possibly a different capitalisation.

## write_text_file

Creates a test file in one line, including its parent directories, and optionally backdates it.
Which of two files is the newer one is the input of almost every sync decision, so the tests need
an mtime they control rather than "both written in the same millisecond" - the age is subtracted
from the current time because only the relative order matters.
