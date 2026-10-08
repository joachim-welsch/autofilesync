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

## RemoveFileReturnsWhetherSomethingWasActuallyRemoved

Fehler 5 (round 6): remove_file() discarded fs::remove()'s own return value and always reported
success regardless of whether anything was actually deleted - e.g. for an SSH "account:/path"
string misinterpreted as a bogus local path, or any other path that simply didn't exist.
