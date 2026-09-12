# DifficultFilenameTests

Module comment for DifficultFilenameTests.cpp.

## BASE_DIR

All three directories live under the scratch directory (a subdirectory of /tmp) and are
ASCII-only themselves - the difficult characters sit exclusively in the file names below them
(DIFFICULT_NAMES), never in the directory path. Every wstring conversion of the directory path
therefore stays uncritical; only the native (UTF-8) file name itself has to survive the special
cases.

## DIFFICULT_NAMES

Deliberately as difficult as possible while still valid on Linux (only '/' and NUL are genuinely
forbidden in a file name there): very long, with spaces (leading, repeated, trailing), shell
metacharacters including an embedded backslash, a hidden dotfile, a name made of dots only,
Unicode/emoji, an embedded line break, a trailing dot, and a leading hyphen (which a shell could
mistake for an option).

## expect_all_present_and_empty

Checks that every difficult name arrived in the target directory as an empty file.

## EmptyFilesWithDifficultNamesSyncToTwoTargets

Copies an empty file under as many difficult file names as possible (long, spaces, special
characters, Unicode, ...) from one source directory into two different target directories under
/tmp and checks that every name arrives intact and empty.

## sync-empty-files-needed

Without this flag 0-byte source files are skipped on purpose (see SyncFileTests: an empty file
otherwise counts as an aborted write) - here they are deliberately empty.
