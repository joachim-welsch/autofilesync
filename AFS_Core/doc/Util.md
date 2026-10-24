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

to_unix_nanoseconds()/from_unix_nanoseconds() are the lossless pair for a timestamp that has to
survive a round trip through a file (SyncState): to_time_t() drops the fraction of a second,
and the sync state has to tell an unchanged file from one written a moment later.

## file-time-overflow

from_unix_nanoseconds() narrows to the resolution of file_time_type before it changes the clock.
On Windows the file clock counts from 1601, and 1601 to today in nanoseconds does not fit into
64 bits: converting in nanoseconds overflowed and put every stored time centuries off, so the
sync state saw every file as changed.

## utf8-files

The configuration and the backup proofs are JSON, once written by boost's write_json() into a
wide stream. A std::wofstream converts through the C++ global locale, which stays "C" whatever
setlocale() chose, and write_json() leaves characters up to U+00FF unescaped: on Linux and macOS
the first umlaut made the stream fail, so a configuration with an umlaut in a name or path could
not be saved. These two functions convert explicitly instead, so the file is UTF-8 everywhere.

Reading old files: on Windows the wide stream wrote such characters as single bytes in the ANSI
code page, and string_to_wstring() falls back to exactly that code page for invalid UTF-8, so
those files keep their umlauts. Elsewhere an old file could only ever be ASCII.

write_utf8_file() refuses an empty conversion of a non-empty text - WideCharToMultiByte() returns
nothing for an unpaired surrogate - because the caller renames the result over the real file,
and an empty configuration would replace the user's synchronisations.

## utf8-line-ends

write_utf8_file() writes binary, so a line ends in "\n" on every platform and a file is the same
bytes wherever it was written; in text mode Windows turned every "\n" into "\r\n".
read_utf8_file() stays in text mode, which still reads files written that way.

## as_file_name

Turns a name the user chose into something every platform accepts as a file name: the characters
Windows forbids become underscores, and trailing dots and blanks go because Windows does not
preserve them. The result may be empty; the caller decides the fallback, since a backup proof and
a sync state want different ones.

## host_name

The name of this computer, for the conflict copies a two-way sync leaves behind
(SyncFile.md#conflict-copy-name): a user who syncs a laptop and a desktop wants to see which of
them held the losing version. gethostname() on POSIX, GetComputerNameA() on Windows, where
gethostname() would need Winsock started first; "localhost" when neither answers.
