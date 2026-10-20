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

## target-only-directories

A directory that exists only in the target is mirrored into the source, every level of it
(SyncTree.md#target-only-directories), while the target keeps what it has and an excluded
directory stays where it is. Three copies: the source's root file out, the two target files
back. A second pass is quiet.

## deleted-directories

With a state, a directory deleted on one side is removed on the other, down to the directory
itself, in both directions, and the state forgets everything below it; a second pass must not
bring it back, and the pass that carries the deletion must not recreate the directory on the
side that dropped it (SyncTree.md#prepare-and-settle). A file edited since the deletion comes
back with its directory while its untouched siblings go, and files added on the surviving side
after the deletion still travel - the directory they live in is not deleted while it holds
them.

## state

The tests with a SyncState (SyncRunner.md#run_sync-with-state) are the reason the state exists:
a file deleted on one side is deleted on the other instead of coming back, in both directions
and for a file item as well. DeleteMode::Permanent, because a recycle bin under the test
directory would show up as a file the pass sees. The first-pass test pins down what enters the
state - every file that was compared or copied, keyed relative to the item's source with '/',
not the excluded file - and that a copy records the target's own write time.

## edit-beats-deletion

A deletion on one side and an edit on the other since the last pass: the edit wins and the
file is copied, in either direction (FileDifference.md#three-way). Ages of two hours and then
zero keep every write time apart by more than the tolerance.

## both-modified

Edited on both sides since the last pass, with the target the newer one: without the state the
target would win and the source edit be lost. The state keeps the entry of the last agreed
version (two bytes, "v1"), so the conflict is reported again on the next pass.

## changed-side-wins

The source is rewritten with a write time three hours back - older than the target's copy -
as a restore from an archive would leave it. The plain comparison would call the target newer
and copy it back over the edit; with the state the target is unchanged and the source is what
changed, so the edit reaches the target, and a second pass is quiet.

## renames

A file renamed or moved on one side is renamed on the other (SyncFile.md#renames), in both
directions and across directories. That it was renamed and not copied is read off the inode,
which a copy cannot keep; on Windows the inode is 0 and only the contents are checked. The
names are chosen so that the old name sorts before the new one in one test and after it in
another, since the detection must not depend on which of the two the pass meets first
(SyncFile.md#deferred-deletions).

## renames-ambiguous

Two files with the same size and write time: when one of them is renamed the state cannot say
which, so the renamed file is copied and the old name deleted - nothing lost, nothing
mistaken.

## one-way-moves

A one-way item that tracks moves (SyncItem.md#track_moves_) and deletes in the target renames
there too. The state must not change what one-way means otherwise
(FileDifference.md#base-only-two-way): a target the user deleted is copied again, a source
file that disappears leaves the target alone without delete_in_target, and nothing in the
source is ever deleted.
