# SshPathEncodingTests

Module comment for SshPathEncodingTests.cpp.

## overview

Reproduces the sync abort logged as Error  /home/welschj/Schreibtisch  mmserver:/home/welschj/
wstring_convert::to_bytes SyncItem::source()/target() and SshPath::path() rebuild an SSH path
with std::wstring(bytes.begin(), bytes.end()). char is signed, so every byte of a non-ASCII
UTF-8 sequence widens to a negative wchar_t (0xc3 -> 0xffffffc3), a value codecvt_utf8 cannot
encode. The next wstring_to_string() of that path - and every log line does one - throws
std::range_error("wstring_convert::to_bytes"). SyncFile's own catch handler logs the target as
well, so the exception escapes from there and takes the whole directory branch with it.

## UNREACHABLE_HOST

RFC 2606 reserves .invalid, so the name never resolves: the tests cannot reach a real host or
write to the user's known_hosts. The defect fires before any connection attempt anyway.

## TearDown

Config is a process-wide singleton: an account left behind would change which paths the
following tests resolve as SSH paths.

## SshAccountNameWithUmlautStaysConvertible

SshPath::path() widens the account name the same way, so an umlaut in the account name alone is
enough - no matter what the remote path looks like.

## TreeSyncToSshTargetDoesNotAbortOnUmlautFilename

The reported failure end to end: one file with an umlaut in its name makes SyncTree report the
conversion error for the whole item instead of syncing.
