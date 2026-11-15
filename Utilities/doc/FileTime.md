# FileTime

Module comment for FileTime.h / FileTime.cpp.

## conversions

The epoch of file_time_type is neither the Unix epoch nor the same on every platform, so a
conversion has to go through the clocks rather than through the raw duration. Both directions
are needed wherever a foreign timestamp meets a local one: SFTP reports seconds since the Unix
epoch, and that is also what is sent back to a server.

to_unix_nanoseconds()/from_unix_nanoseconds() are the lossless pair for a timestamp that has to
survive a round trip through a file (SyncState): to_time_t() drops the fraction of a second,
and the sync state has to tell an unchanged file from one written a moment later.

equal() and greater() compare two file times; on macOS they go through the system clock, because
Apple's libc++ has no clock_cast.

## overflow

from_unix_nanoseconds() narrows to the resolution of file_time_type before it changes the clock.
On Windows the file clock counts from 1601, and 1601 to today in nanoseconds does not fit into
64 bits: converting in nanoseconds overflowed and put every stored time centuries off, so the
sync state saw every file as changed.
