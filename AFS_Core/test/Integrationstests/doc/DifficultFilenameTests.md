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

## windows-names

Win32 cannot create most of the Linux-only names: `<>:"/\|?*` and control characters such as the
line break are forbidden, and a trailing space or dot is silently stripped - such a file would
exist under a different name, so the test would compare two normalised paths instead of testing
anything. Windows therefore keeps only the categories it can represent. The long name is shorter
because the application is not long-path aware: temp directory, BASE_DIR and the name must stay
below MAX_PATH (260), otherwise the source file cannot even be created.

## utf8-paths

The names are UTF-8 literals (/utf-8 on MSVC). A plain fs::path(std::string) interprets narrow
strings in the ANSI code page on Windows, so a Unicode name would land on disk garbled - and the
check would pass anyway, because it garbles the name the same way. Going through std::u8string
states the encoding explicitly and is correct on every platform.

## expect_all_present_and_empty

Checks that every difficult name arrived in the target directory as an empty file.

## EmptyFilesWithDifficultNamesSyncToTwoTargets

Copies an empty file under as many difficult file names as possible (long, spaces, special
characters, Unicode, ...) from one source directory into two different target directories under
/tmp and checks that every name arrives intact and empty.

## sync-empty-files-needed

Without this flag 0-byte source files are skipped on purpose (see SyncFileTests: an empty file
otherwise counts as an aborted write) - here they are deliberately empty.
