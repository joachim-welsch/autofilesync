# FilesystemUtil

Module comment for FilesystemUtil.h / FilesystemUtil.cpp.

## unix-style-paths-on-windows

Handle Unix-style paths (e.g., "/home/...") on Windows These can occur when using WSL paths or
cross-platform test data

## home_dir-windows

USERPROFILE comes first: it is set for every process of the user, the background service
included. HOMEDRIVE and HOMEPATH are set by the interactive logon only, so the service found no
home directory and put the recycle bin beside its working directory, apart from the one the
program uses. They remain as the fallback.
