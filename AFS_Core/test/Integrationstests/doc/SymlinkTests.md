# SymlinkTests

Module comment for SymlinkTests.cpp.

SyncTree.md#symlinks-never-followed describes the rule; SyncTreeTests only checked that a
symlink cycle does not crash. These tests pin what the user sees: which links are skipped,
which are copied, and that a skipped link is named in the log. Unix only - symlinks need
privileges on Windows.

## SymlinkTest

One source tree with a real directory, a real file and one symlink to each; the log is
redirected so the tests can count the "symlink not followed" lines. The directories are
opened up and the files made world-readable because one test reads the same tree through
the SSH user.

## skipped_links

The message is logged with the path as a second argument, so the search text is the
translated message alone; translate_utf8() gives the same text the logger used whether or
not a catalogue is loaded.

## TreeSyncSkipsADirectorySymlinkAndSaysSo

The directory link is skipped and logged once; the real directory it points to is still
synchronised under its own name, so nothing is lost, only not duplicated.

## file-symlinks-are-content

Locally a symlink to a file is a file: is_regular_file() follows it, the content is copied,
and the target gets a plain file - a link in the target would point back into the source.
SyncDir treats a directory symlink like any directory, i.e. it ignores it without a log line.

## TreeSyncFromSshSkipsEverySymlink

An SFTP listing reports a symlink only as a symlink, not what it points to, so over SSH the
file link is skipped as well, and both links are named in the log.
