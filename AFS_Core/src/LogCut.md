# LogCut

Module comment for LogCut.h / LogCut.cpp.

## temp-file-created-late

The temp file is created only once the log itself opened: on the early return the caller skips
close_files(), and with it the rename that turns the temp file back into the log - creating it
eagerly left an orphaned ".tmp" behind on every start without an existing log file.

## error-code-overload

The error_code overload: this runs at every start, and a log directory that became read-only
meanwhile must not take the application down.

## reopen-log-handle

open_files() closed the log handle and only close_files() reopens it: without this, a first
start (no log file to cut yet) left write() silently dropping everything for the rest of the
session.

## per-line-age-decision

Decided per timestamped line, never latched: logs are chronological (old lines first), so a
sticky flag would discard everything after the first expired line - including all current
entries. Lines without a parseable timestamp (multi-line entries) follow the preceding
timestamped line.
