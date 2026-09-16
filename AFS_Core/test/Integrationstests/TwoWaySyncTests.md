# TwoWaySyncTests

Module comment for TwoWaySyncTests.h / TwoWaySyncTests.cpp.

Two-way sync is the only mode in which the target side may win, so every decision SyncFile makes
has a second outcome here that the one-way tests never reach: copy back instead of copy forward,
keep instead of delete, and - most importantly - stop once both sides agree. SyncDirTests and
SyncTreeTests each cover the single case "a file that exists only in the target"; these tests
cover the whole decision table and the repeated pass. The SSH combinations live in
SyncSshTwoWayTests.

## own-scratch-paths

Its own directory pair below the scratch directory: SyncDirTests and SyncTreeTests share fixed
paths and delete them wholesale, and a test that runs a second pass must be sure nothing else
wrote into them in between.

## synchronize-start

Every sync class returns immediately while Synchronize::stopping() is true, which is the state
of a fresh process - without this the tests would silently pass on an empty sync.

## ScopedDeleteMode

The delete mode is global configuration and other test suites change it, so the test that needs
a permanent delete sets it itself and restores the previous value - otherwise the recycle bin
would keep a copy and the test order would decide where the deleted file ends up.

## back-copy-timestamp

The back copy must take the target's modification time with it. Without that the source looks
newer than the target on the next pass and the same file is copied in the opposite direction
forever - a ping-pong that the repeated-pass tests below detect.

## DeleteInTargetWinsOverTwoWaySyncWhenTheSourceFileIsGone

Both options describe what happens to a file that exists only in the target, and they demand the
opposite: delete_in_target deletes it, two_way_sync copies it back. SyncFile::sync_difference()
lets the deletion win; the combination is reachable in the GUI, so the precedence is pinned here
instead of being rediscovered from the code.

## EmptyTargetOnlyFileIsSkippedUntilSyncEmptyFilesIsSet

The "an empty file is more often an aborted write than a wanted file" rule (see
SyncFile.md#empty-source-file) has to hold for the target-to-source direction too, including the
sync_empty_files escape hatch.

## DirSyncExchangesTheNewerVersionOfEveryFileInOnePass

One pass has to settle the whole directory: files that exist on one side only travel in their
respective direction, and for files on both sides the newer version wins - whichever side that
is. Two-way sync never deletes, so the existing target-only file must also still be there.

## idempotence

The property that makes two-way sync usable at all: once both sides carry the same content, a
further pass must copy nothing. A single copy in the second pass means the sync will repeat it
on every cycle for as long as the item exists.

## BackCopyLeavesNoBackupFileBehindInTheSource

save_copy_file() renames an existing destination to ".bak" before overwriting it, so
restore_backup() can put it back when the copy fails (see SyncFile.md#restore_backup). After a
successful copy that file has no purpose left. It is currently not removed, which is what makes
the repeated-pass tests above fail: with two-way sync the leftover ".bak" is a source-only file
for the next pass and gets copied to the other side.

## TargetOnlyDirectoryIsLeftAloneByTwoWaySync

Pins the deliberate boundary that SyncTreeTests.md#DeleteInTargetLeavesTargetOnlyDirectoriesAlone
describes for the deletion side: the target-only pass runs per file at each recursion level, and
SyncTree never descends into a directory that exists only in the target. Nothing there is lost -
it simply does not travel back.
