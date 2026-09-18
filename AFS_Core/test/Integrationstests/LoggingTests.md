# LoggingTests

Module comment for LoggingTests.cpp.

## single-log-entry

Only one log entry may exist for the drive that is not mounted. The search looks for a
characteristic text that is written whenever an unmounted drive is checked.

## LogOriginTest

Redirects the log to a scratch file and restores path, level and origin
afterwards, since Logging is a process-wide singleton the other tests share.
The assertions compare everything behind the timestamp, so they pin the exact
place of the marker: between timestamp and level.
