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
