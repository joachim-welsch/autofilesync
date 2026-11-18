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

## remove_refusal

Removes a file and says why when that fails, where remove_file() only says that it did: the
deletion of a sync needs the reason for its report (SyncBase.md#deleted). A file that was not
there is FileNotExist (remove-returns-false); a failure the system gives no known reason for is
a write error (reason_for_write_failure). Nothing is logged here, the caller words the message.

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

## reason_for_missing

Why fileinfo() found no file, in the order that matters to the synchronisation. An unmounted
drive comes first: everything below it is missing, and none of it was deleted. Then the
directory the file should lie in: DirectoryNotExist when it is missing or is not a directory.
Only a file that is missing from a directory that is there is FileNotExist. A name without a
directory part lies in the current directory, which exists.
A directory that refuses the question is PermissionDenied, not a missing one.

## reason_for_failure

The reason behind a system error, for fileinfo() and for everything that opens, writes or
renames a file. FileLocked when another program holds the file (lock_error); DiskFull when the
drive or the user's quota has no room left (space_error); ReadOnlyFilesystem when the drive
takes no writes at all (read_only_error); PermissionDenied when the
system refused, as a directory without search permission does for everything below it;
FileNotExist when the file is gone; ErrorReadFileInfo for any other failure. Only FileNotExist
says anything about the existence of the file, and the caller must not read the others as
"missing" (FileDifference.md#not-readable).

## read_only_error

"Read-only file system" is what every change answers on a medium that takes none: a card with
its write-protect switch set, a CD, an image, a drive the system remounted read-only after an
error. It is a reason of its own because it holds for the whole drive, not for one file - the
caller can stop instead of failing file by file (CopyLog.md#abandon) - and because "permission
denied" would send the user looking at rights that are not the cause. Windows reports
ERROR_WRITE_PROTECT, which the portable conditions fold into "permission denied"; so the check
stands before the one for permissions.

## read_only_filesystem

Whether the drive a path lies on is mounted read-only, asked before anything is tried
(Synchronize.md#write-protected, RecycleBin.md#read-only-source). A path that does not exist
yet is measured by the part of it that does, since the question is about the drive the file
would be created on. No answer - the drive is gone, the path is relative and empty - is
"writable": the attempt then tells. Tests name a directory tree that counts as read-only,
because a file system cannot be mounted read-only for a test without root
(ReadOnlyFilesystemTests.md#ReadOnly); the override exists in test builds only. It changes the
answer of this question alone - the files below can still be written, which is how a test
sees that nothing was tried.

## reason_for_write_failure

The same for an attempt to change something: a rename, a write, a new file time. The reasons
the system names stay what they are; only the rest is ErrorWriteFileInfo instead of
ErrorReadFileInfo, so that the user is not told of a read error where nothing was read.

## lock_error

On Windows a program that opens a file can keep all others out; the system then answers
ERROR_SHARING_VIOLATION for the whole file or ERROR_LOCK_VIOLATION for a locked range. The
portable error conditions map both to "permission denied", so the system's own codes are
looked at first. Unix locks are advisory and do not stop a read; the codes that come closest
are "text file busy" and "device or resource busy".

## last_stream_error

The error behind a failed fopen() or fread(). On Windows the C runtime keeps the system's code
in _doserrno next to the errno it derives from it; errno alone would no longer tell a lock
from a missing right.

## read_refusal

Why a file does not open for reading, or OK when it does. Asked before a copy moves the old
target aside (SyncFile.md#refused): a file can have a size and a time and still not open.

## compare_content

Compares two files block by block and keeps apart what files_equal() cannot: "equal" false
with refusal OK means both were read and differ; with any other refusal one of them could not
be opened or read, refused_file says which, and nothing is known about their contents.
files_equal() remains for callers that verify a copy they have just made, where "could not
read it back" and "differs" are the same failure.

## rename_refusal

rename_file() with the reason for a failure, so that the caller can tell the user that the
file is locked or the directory write-protected instead of only that something failed.
A failure the system gives no known reason for is a write error (reason_for_write_failure).
Tests set an error for chosen renames - so many succeed first, then so many fail - because
the second of two renames in one directory cannot be made to fail by rights or a lock while
the first goes through (SymlinkAsLinkTests.md#AnOldTargetThatCannotComeBackIsNamedWhereItLies);
the override exists in test builds only.

## write_time_refusal

Sets the last write time and says why when that fails, where set_last_write_time() only says
that it did. The copy needs the reason for its report (SyncFile.md#file-time-failure). A file
the user owns never refuses a new time, so tests set the error instead: in test builds every
call answers with the reason for that error and leaves the file alone until the override is
taken back.

## failure_text

The words for an error code in a report. The reasons the program knows are said in the user's
language (reason_for_failure); for the rest the system's own text is kept, because "No space
left on device" helps where "unknown error" does not. Only no error code at all is the unknown
error.

## create_directory_refusal

Creates a directory with every missing level and says why when it is not there afterwards:
PermissionDenied, FileLocked, DiskFull or ReadOnlyFilesystem when the system gives that reason, CannotCreateDirectory for
everything else - most often a plain file that bears the name of the directory or of a level
above it. create_directories() does not agree with itself across platforms on whether that
case is an error, so the result is checked: no directory, no success. The error code goes back
for failure_text().

## space_error

"No space left on device" and a used-up quota are the same thing to the user: the file does
not fit, and room has to be made. Windows has codes of its own for both, which the portable
conditions do not all cover.

## free_space

The bytes a file may still take on the drive a directory lies on - what is available to the
user, which can be less than what is free. No answer (the directory is not there, the drive
does not say) is not zero: the caller then simply tries. Tests set the answer, because a disk
cannot be filled for a test (DiskFullTests.md#FreeSpace); the override exists in test builds
only and holds for every directory until it is taken back.

## permissions_refusal

Sets the permission bits of a file and says why when that fails: the reason the system names,
or CannotSetFileAttributes for the rest. A file system that answers "not supported" keeps no
such rights, so there are none to lose and the answer is OK - otherwise every copy onto such
a drive would be an error. Like the file time, the rights of a file the user owns are never
refused, so tests set the error (write_time_refusal).
