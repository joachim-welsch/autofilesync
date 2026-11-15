# FileNameTests

Module comment for FileNameTests.cpp.

## TimestampPrefixCarriesTheMilliseconds

Only the millisecond part is compared: the rest of the prefix is local time and depends on the
time zone of the machine running the test.

## TimestampPrefixNeedsAFileName

Prefixing a path without a file name would rename the last directory instead.
