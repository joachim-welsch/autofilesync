# LockedFileTests

Module comment for LockedFileTests.cpp. Windows only: there a program that opens a file can
keep every other program from opening, renaming or deleting it. Unix has no such locks; what
the two platforms share - a file that is there but does not open - is tested with a file
without read permission in LocalPermissionTests.md and FilesystemUtilTests.md.

## ExclusiveHandle

The other program: it holds the file open without sharing it, as an office program does with
a document or a mail program with its mailbox, until it goes out of scope.

## LockedFileTest

A directory pair of its own and a File item over "a.txt", compared by time or by content. The
report must carry exactly one error that names the locked file and says that it is locked.

## LockedFileIsReportedAsLockedNotAsDifferent

FileLocked is the reason, not PermissionDenied, which the same system error would otherwise
map to: the user can do something about a lock - close the other program - that they cannot
do about a missing right. Size and time of a locked file are still answered, so fileinfo()
does not see the lock; opening the file does.

## ContentComparisonWithALockedFileChangesNothing

The counterpart of LocalPermissionTests.md#ContentComparisonWithAFileThatDoesNotOpenChangesNothing
with a real lock: two identical files, one of them locked, one-way and two-way. Nothing is
copied or renamed, and the pass after the lock is gone finds them equal.

## LockedSourceIsNamedAndTheTargetStays

The source is newer and locked. The target must not be moved aside for a copy that cannot
happen; once the lock is gone the copy goes through.

## LockedTargetIsNamedAndStays

The target is locked: moving it to ".bak" fails. The error names the target and the lock
instead of a bare "copy failed", and both files stay as they are.
