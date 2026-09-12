# LogCutTests

Module comment for LogCutTests.cpp.

## local_timestamp

get_time_from_line() parses via mktime (local time), so the fixture timestamp must be formatted
in local time as well.

## KeepsRecentLinesAfterExpiredOnes

Fehler 8 (round 4): cut() latched its "too old" flag on the first expired line and never reset
it. Logs are chronological (oldest lines first), so as soon as the retention span was ever
exceeded, the whole log - including all current entries - was discarded on every program start
(MainWindow runs LogCut in its constructor). Expiry must be decided per timestamped line;
timestamp-less continuation lines follow the preceding timestamped line.

## own-block-for-windows

A block of its own: Windows refuses to delete a file that is still open (see the fs::remove() at
the end), POSIX unlinks it without complaint.
