# FileTimeTests

Module comment for FileTimeTests.cpp.

## FileTimeRoundTripsThroughTimeT

The epoch of file_time_type differs from the Unix epoch on libstdc++; a conversion through the
raw duration instead of through the clocks would shift every SFTP timestamp by decades.

## FileTimeRoundTripsThroughUnixNanoseconds

The pair the sync state relies on (FileTime.md#overflow): a file time has to come back
exactly, or the next run takes every file for changed. The moment is a multiple of 100 ns, the
resolution of the Windows file clock, so it survives there too.
