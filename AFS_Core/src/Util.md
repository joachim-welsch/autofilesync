# Util

Module comment for Util.h / Util.cpp.

## to_uint16_t

Throws std::invalid_argument or std::out_of_range like std::stoi, so a configuration value
outside 0..65535 is rejected instead of truncated.

## with_timestamp_prefix

Puts "YYYY-MM-DD_hh-mm-ss-mmm_" in front of the file name, leaving the directory part untouched.
The time defaults to the moment of the call; pass one explicitly to get a reproducible name.

## with_trailing_char-empty-input

s.empty() must be checked first: *rbegin() on an empty string is UB (hard iterator-debug assert
on MSVC). Empty input keeps the contract "result ends with c".

## timestamp

floor() instead of system_clock::to_time_t(): to_time_t may round to the nearest second, which
would pair a rounded-up second with the milliseconds of the original one and yield a timestamp
that never existed.

## filename-required

A path ending in a separator (or an empty one) has no file name to prefix; prefixing the last
directory instead would silently rename it.

## wildcard-escaping

Everything except the wildcard metacharacters * and ? must be escaped: an unescaped '.' made
"*.txt" match "abXtxt", parentheses turned into regex groups so "Fotos (2020)*" never matched
its own files, and an unbalanced '[' or '(' threw std::regex_error on every comparison.

## remove-returns-false

fs::remove() answers false when there was nothing to remove, which is not an error but must not
be reported as a deletion either.

## file-time-conversions

The epoch of file_time_type is neither the Unix epoch nor the same on every platform, so a
conversion has to go through the clocks rather than through the raw duration. Both directions
are needed wherever a foreign timestamp meets a local one: SFTP reports seconds since the Unix
epoch, and that is also what is sent back to a server.

