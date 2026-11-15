# FileName

Module comment for FileName.h / FileName.cpp.

## with_timestamp_prefix

Puts "YYYY-MM-DD_hh-mm-ss-mmm_" in front of the file name, leaving the directory part untouched.
The time defaults to the moment of the call; pass one explicitly to get a reproducible name.

## timestamp

floor() instead of system_clock::to_time_t(): to_time_t may round to the nearest second, which
would pair a rounded-up second with the milliseconds of the original one and yield a timestamp
that never existed.

## filename-required

A path ending in a separator (or an empty one) has no file name to prefix; prefixing the last
directory instead would silently rename it.

## sanitized

Turns a name the user chose into something every platform accepts as a file name: the characters
Windows forbids become underscores, and trailing dots and blanks go because Windows does not
preserve them. The result may be empty; the caller decides the fallback, since a backup proof and
a sync state want different ones.
