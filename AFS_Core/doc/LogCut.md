# LogCut

Module comment for LogCut.h / LogCut.cpp.

## temp-file-created-late

The temp file is created only once the log itself opened: on the early return the caller skips
close_files(), and with it the rename that turns the temp file back into the log - creating it
eagerly left an orphaned ".tmp" behind on every start without an existing log file.

## error-code-overload

The error_code overload: this runs at every start, and a log directory that became read-only
meanwhile must not take the application down.

## replace_log_file

The GUI program and the daemon write to the same file, and either of them may be cutting it
while the other holds it open. On POSIX the shortened copy is renamed over the log - atomic, and
Logging's follow_replaced_file() lets the other process pick up the new file. Windows refuses to
rename over a file another process holds open ("access denied"), so there the shortened copy is
written back into the existing file: truncating it is allowed, because fopen shares the file for
writing, and the other process appends behind the new end from then on, as its handle is in
append mode and the file keeps its identity. A line the other process writes while the copy is
in progress lands between the old lines - rare, once a day, and harmless.

## reopen-log-handle

open_files() closed the log handle and only close_files() reopens it: without this, a first
start (no log file to cut yet) left write() silently dropping everything for the rest of the
session.

## per-line-age-decision

Decided per timestamped line, never latched: logs are chronological (old lines first), so a
sticky flag would discard everything after the first expired line - including all current
entries. Lines without a parseable timestamp (multi-line entries) follow the preceding
timestamped line.

## DailyLogCut

The GUI cuts the log at every start. The daemon runs for weeks, so it has to cut
while running: it calls this once per pass of its loop, and the log is cut at the
first call and then whenever 24 hours have gone by. system_clock rather than
steady_clock, because the steady clock stands still while a laptop is suspended
and the day would never end. The time is a parameter only for the tests.
