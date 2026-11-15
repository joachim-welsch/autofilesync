# StrUtil

Module comment for StrUtil.h / StrUtil.cpp.

## utf8-conversions-never-throw

Hand-written instead of std::wstring_convert: that one throws std::range_error on the first byte
that is not valid UTF-8, and file names on Linux are arbitrary bytes. These conversions run
inside every log call, including the ones in catch handlers, so they must never throw; invalid
input becomes U+FFFD instead.

## decode_utf8

Decodes one code point starting at s[pos] and advances pos past it.

## to_uint16_t

Throws std::invalid_argument or std::out_of_range like std::stoi, so a configuration value
outside 0..65535 is rejected instead of truncated.

## to_string-file-time

The time in UTC as "weekday, day month year hh:mm", for log lines. The conversion to a calendar
time goes through util::to_time_t() (FileTime.md#conversions).

## trimmed

Removes spaces, tabs, carriage returns and line feeds from both ends; text that consists of
nothing else becomes empty.

## lowercase

The narrow overload lowers ASCII letters only and leaves the bytes of UTF-8 sequences alone - it
is meant for protocol names such as HTTP header fields. The wide overload uses towlower(), which
lowers non-ASCII letters only in a UTF-8 locale - which the programs set in init_locale(); in the
"C" locale an uppercase "Журнал" would stay as it is.

## split

Empty parts are dropped, so leading, trailing and doubled separators yield no entries: "/a//b/"
split at '/' is {"a", "b"}.

## unquoted

Removes one pair of surrounding double quotes and nothing else; whitespace around the quotes has
to be trimmed beforehand.

## with_trailing_char-empty-input

s.empty() must be checked first: *rbegin() on an empty string is UB (hard iterator-debug assert
on MSVC). Empty input keeps the contract "result ends with c".

## wildmatch

True if the whole input matches the wildcard, where * stands for any number of characters and ?
for exactly one; the comparison is case-sensitive.

## wildcard-escaping

Everything except the wildcard metacharacters * and ? must be escaped: an unescaped '.' made
"*.txt" match "abXtxt", parentheses turned into regex groups so "Fotos (2020)*" never matched
its own files, and an unbalanced '[' or '(' threw std::regex_error on every comparison.
