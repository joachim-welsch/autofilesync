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
