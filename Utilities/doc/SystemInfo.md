# SystemInfo

Module comment for SystemInfo.h / SystemInfo.cpp.

## host_name

The name of this computer, for the conflict copies a two-way sync leaves behind
(SyncFile.md#conflict-copy-name): a user who syncs a laptop and a desktop wants to see which of
them held the losing version. gethostname() on POSIX, GetComputerNameA() on Windows, where
gethostname() would need Winsock started first; "localhost" when neither answers.
