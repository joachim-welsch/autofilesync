# StrUtilTests

Module comment for StrUtilTests.cpp.

## WstringToStringRoundTripsUmlauts

Regression test: wstring_to_string() used to truncate/corrupt non-ASCII characters instead of
UTF-8-encoding them, so anything with umlauts (SSH paths, host/user/password, filenames) got
silently mangled on Linux/macOS.

## WithTrailingCharHandlesEmptyString

Regression test (Fehler 4, round 4): with_trailing_char() dereferenced rbegin() without checking
for an empty string - undefined behavior (a hard iterator-debug assert on MSVC). Reachable from
every path-joining call site whenever a source/target ends up empty (bad config entry, mangled
path).

## WildmatchTreatsNonWildcardCharsLiterally

Fehler 2 (round 5): wildcard_to_regex() only replaced * and ? but escaped nothing else, so the
remaining characters were interpreted as regex: "*.txt" (-> ".*.txt") matched "abXtxt",
parentheses became groups so "Fotos (2020)*" never matched its own files, and an unbalanced "["
threw std::regex_error on every comparison, silently aborting the directory sync.

## ToUint16RejectsValuesOutsideTheRange

A port of 65536 must not silently become port 0. The last value does not even fit into the int
std::stoi() parses into, so the exception comes from there instead of from the range check.
