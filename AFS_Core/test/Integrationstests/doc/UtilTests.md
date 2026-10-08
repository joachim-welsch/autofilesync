# UtilTests

Module comment for UtilTests.cpp. The string helpers of Util.h are covered by StrUtilTests.cpp.

## COMPARE_BLOCK

files_equal() reads both files in blocks of this size. Contents of several blocks make sure the
loop goes on behind the first block, and a size that is an exact multiple of the block hits the
iteration in which both reads return nothing.

## ToUint16RejectsValuesOutsideTheRange

A port of 65536 must not silently become port 0. The last value does not even fit into the int
std::stoi() parses into, so the exception comes from there instead of from the range check.

## FileTimeRoundTripsThroughTimeT

The epoch of file_time_type differs from the Unix epoch on libstdc++; a conversion through the
raw duration instead of through the clocks would shift every SFTP timestamp by decades.

## TimestampPrefixCarriesTheMilliseconds

Only the millisecond part is compared: the rest of the prefix is local time and depends on the
time zone of the machine running the test.

## TimestampPrefixNeedsAFileName

Prefixing a path without a file name would rename the last directory instead.

## RemoveFileFailsForANonEmptyDirectory

fs::remove() throws here instead of answering false; remove_file() has to turn that into false
without deleting anything.
