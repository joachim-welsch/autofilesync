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

## CutsWhileAnotherProcessHoldsTheFileOpen

The GUI cut the shared log while the daemon held it open and logged "Logdatei konnte nicht
gekuerzt werden: Zugriff verweigert": Windows refuses to rename over an open file. A second
handle in append mode stands in for the daemon; after the cut the expired line must be gone,
the temp file gone, no error logged, and a line written through the other handle must still
reach the file - its handle survives the cut.

## own-block-for-windows

A block of its own: Windows refuses to delete a file that is still open (see the fs::remove() at
the end), POSIX unlinks it without complaint.

## DailyLogCutTest

An expired line in the log shows whether a call has cut: it is gone after a cut
and still there otherwise. The time is handed in, so the tests cover a whole day
without waiting. The line is appended with the log handle closed, as the handle
of Logging and the stream of the test must not write to the file at once.
