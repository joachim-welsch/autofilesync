# LocalPermissionTests

Module comment for LocalPermissionTests.cpp.

"Permission denied" on the local filesystem had no test; the SSH side has
FailedSshCopyRestoresBackupAndReportsFailure. Every case here asks the same two questions: is
the old target still intact, and does the run report the failure - a silent skip would let the
proof file claim a successful run. Unix only, and skipped as root, who is not refused anything.

## LocalPermissionTest

A directory pair of its own; the locks are plain chmod. TearDown gives everything owner_all
back before removing it, because remove_all() cannot delete inside a directory the test
locked.

## unlock-before-removing

The locked directory may be the root, the target or the extra "gesperrt" parent, and the
locked file may be unreadable: unlock the known directories first, then whatever the
iterator can now reach.

## run

The errors of a run reach the proof through the SyncReport that SyncReportGuard installs; the
tests install one of their own and read it, like Synchronize::report_sync_item() does.

## ReadOnlyTargetFileIsStillReplaced

The positive case that explains pre_copy(): a read-only target file is the sync's own business
and gets owner permissions before it is replaced, so no error.

## UnwritableTargetDirectoryKeepsTheOldTargetAndReportsTheError

The target directory refuses the rename to ".bak" that save_copy_file() does first. The old
file is untouched, which is right - but the item must still count as failed.

## NewFileInAnUnwritableTargetDirectoryIsReported

Without an existing target the copy itself fails at the open; the message names the target
so that the user knows which directory to fix.

## UnreadableSourceKeepsTheOldTargetAndReportsTheError

The copy fails at the first read; restore_backup() has to put the renamed old target back.

## UncreatableTargetDirectoryIsReportedByName

create_target_dir_if_not_exist() cannot create the target below a locked parent; the first
error names the target directory, the file-level errors that follow are noise behind it.

## UnreadableSourceDirectoryIsReportedAndCopiesNothing

The directory iterator throws before any file is seen; SyncDir catches it once for the whole
item.
