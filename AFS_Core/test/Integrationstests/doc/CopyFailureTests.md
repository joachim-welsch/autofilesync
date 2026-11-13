# CopyFailureTests

Module comment for CopyFailureTests.cpp.

What a local copy or deletion does when it fails half way: LocalPermissionTests covers the
failures that show at the open, these cover the ones that show later - the disk fills up, the
source changes under the copy, the deletion is refused - and the files next to the target that
are none of the sync's business. Unix only: the failures are provoked with resource limits and
chmod.

## CopyFailureTest

A directory pair of its own, like LocalPermissionTest, and its run() with a SyncReport of its
own (LocalPermissionTests.md#run).

## unlock-before-removing

FailedDeletionIsReportedAndCaughtUpOnLater locks the target directory; if it fails before it
unlocks, remove_all() could not delete inside it.

## FileSizeLimit

A full disk without a disk: RLIMIT_FSIZE makes every write beyond the limit fail with EFBIG,
which is what the copy sees when the target runs out of space. The limit holds for the whole
process, so it is set only around the one run. SIGXFSZ, which would otherwise end the test
program at the first refused write, is ignored for that time.

## log-below-the-limit

The limit also applies to the log file. The shared test log may already be longer than the
limit, and its writes would then fail for the length of the run, so the run logs into a fresh
file of its own.

## RestlessWriter

A program that keeps writing into the source while it is copied: a thread overwrites the first
bytes with a rising counter until it is stopped. The copy reads those bytes at its start and
the verification reads them again after the last block, by which time the counter has moved on
- the source is 16 MiB so that the two reads are far apart.

## WriteErrorDuringTheCopyRestoresTheOldTarget

The copy is cut off after the first mebibyte. The half file must go, the old target must come
back from its ".bak", and the run must count as failed.

## NewFileThatCannotBeWrittenLeavesNoHalfFile

The same without an old target: a half file left behind has the size of nothing in particular
and a fresh time, and the next pass by time would take it for the newer side.

## SourceChangedDuringTheCopyRestoresTheOldTarget

The verification after the copy (files_equal) exists for exactly this: a target that differs
from its source is not a copy. It had no test, because the difference needs a source that
moves while it is read.

## UsersOwnBakFileBesideTheTargetSurvivesTheCopy

A file item names one file; whatever else lies in the target directory belongs to the user,
also when it happens to be called like the sync's own parking place, "<name>.bak". Three cases:
the copy succeeds, there is no old target at all, the copy fails. In each the user's file is
still there with its content. These tests found that save_copy_file() deleted such a file
before the copy and once more after it (SyncFile.md#free_backup_name).

## FailedDeletionIsReportedAndCaughtUpOnLater

A deletion that the state asks for and the file system refuses. The run must say so, must not
copy the file back to the side where the user deleted it, and must keep the entry in the
state: without it the file would count as new in the target on the next pass and travel back
to the source. Once the directory is writable again the deletion is caught up on.
