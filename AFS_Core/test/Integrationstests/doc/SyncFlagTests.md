# SyncFlagTests

Module comment for SyncFlagTests.cpp.

Three options of the sync item dialog decide whether data is deleted or overwritten and had no
test that actually ran them: "delete after copy" (a move), "copy when the target is newer" and
the father-son principle. The planning and summary tests only look at the flag; these tests run
the sync and look at the files.

## SyncFlagTest

Its own directory pair below the scratch directory, like TwoWaySyncTests, so a second pass can
be sure nothing else wrote into them. The sync classes return at once while
Synchronize::stopping() is true, which is the state of a fresh process, hence the start() call.

## MoveDeletesTheSourceAfterTheCopy

The order matters: the source goes only after the copy succeeded (see
StopDuringAMoveKeepsTheSource in SyncFileTests). Permanent delete mode, so the test does not
depend on where the recycle bin lives.

## MoveSendsTheSourceToTheRecycleBinInRecycleBinMode

In the default mode the deleted source has to be recoverable like every other deletion of the
program, so it must end up in the (scoped) recycle bin.

## MoveLeavesAnUnchangedSourceAlone

A move is "copy, then delete": identical files are not copied, so nothing may be deleted
either - otherwise a second pass of a move item would wipe the source without a copy.

## MoveEmptiesTheSourceDirectoryFileByFile

The flag travels with the per-file SyncItem that SyncBase::sync_file() builds, so a Dir item
moves every file; the directory itself is not part of the item and stays.

## NewerTargetIsLeftAloneWithoutCopyWhenTargetNewer

The baseline: by time, a newer target is DestModified and a one-way sync does nothing with it.

## CopyWhenTargetNewerOverwritesTheNewerTarget

With the option the source wins even against a newer target; the copy carries the source's
content and time, so the pair is settled afterwards.

## CopyWhenTargetNewerWinsOverTwoWaySync

SyncFile::decide() tests the copy-to-target conditions before the two-way copy back, so both
options together overwrite the newer target instead of copying it back. Pinned down here as the
current behaviour: whoever changes the order will see this test, not a surprised user.

## FatherSonKeepsTheOldTargetAsFirstGeneration

pre_copy() renames the old target to ".bak.1" before the copy; save_copy_file() then finds no
target and creates no ".bak" of its own, so exactly one copy of the old content remains.

## FatherSonNumbersEveryGeneration

The number is the first free one, so earlier generations are never overwritten.

## FatherSonNeedsNoBackupForANewTarget

Nothing to keep, nothing to rename.

## FatherSonBackupsSurviveDeleteInTarget

The generations exist only in the target. Without special treatment the target-only pass
(SyncBase::sync_target_only_entries) would treat ".bak.1" as a file the source no longer has and
delete it in the very same pass that created it - father-son and delete-in-target would cancel
each other out. Real target-only files must still go.

## FatherSonBackupsAreNotCopiedBackByTwoWaySync

The same target-only pass copies target-only files back to the source in a two-way sync; the
generations must not spread to the source that way.
