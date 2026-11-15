# FilesystemUtil

Module comment for FilesystemUtil.h / FilesystemUtil.cpp.

## unix-style-paths-on-windows

Handle Unix-style paths (e.g., "/home/...") on Windows These can occur when using WSL paths or
cross-platform test data

## plain_entry_name

Whether a name an SFTP server listed can be appended to a local or remote directory path as
one entry: not empty, not "." or "..", and without "/", "\" or NUL. A real file system never
lists anything else, so a name that fails is a server that lies; appended unchecked, a name
like "../../.ssh/authorized_keys" would carry the synchronisation out of the configured tree
(the scp and rsync clients had this very hole, CVE-2019-6111 and CVE-2024-12087). The backslash
counts on every platform because the name may be joined to a Windows path.

## env_path

The value of an environment variable as a path, or nothing if the variable is unset or empty.
On Windows it reads the wide CRT environment: the narrow one is converted to the ANSI code
page, which cannot represent every user name that appears in LOCALAPPDATA or USERPROFILE.

## home_dir-windows

USERPROFILE comes first: it is set for every process of the user, the background service
included. HOMEDRIVE and HOMEPATH are set by the interactive logon only, so the service found no
home directory and put the recycle bin beside its working directory, apart from the one the
program uses. They remain as the fallback.

## remove-returns-false

fs::remove() answers false when there was nothing to remove, which is not an error but must not
be reported as a deletion either.

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

## private-files

write_private_file() creates a file that only its owner may read: the password key, the
device's TLS private key and the pairing token. On POSIX the file is opened with mode 0600, so
it is owner-only from the moment it exists; a chmod() after an ordinary ofstream would leave
a window in which the umask, usually 022, made the secret readable for every account on the
machine. fchmod() also narrows a file that an earlier version left wider. On Windows the file
inherits the ACL of the user's profile directory, which already excludes other users.
