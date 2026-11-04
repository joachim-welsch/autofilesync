# SyncStateRunTests

Module comment for SyncStateRunTests.cpp.

## SyncStateRunTest

The sync state as Synchronize handles it (Synchronize.md#sync-state), where TwoWaySyncTests
only drive the sync classes with a state in hand: here the state is found next to the
configuration, read before the pass, written after it and dropped with the item. The
configuration directory is redirected into the test directory (ScopedConfigHome.md), so no
state file reaches the user's real configuration, and the log goes there too; everything
vanishes with the directory. The logging guard is reset in TearDown, because Windows refuses
to remove the directory while the log file in it is open. Deletions are permanent, because a recycle bin inside the test
tree would be synchronised like any other directory.

## run_pass

A Synchronize of the given type runs on its own thread. A real pass refreshes the item's
last_sync when it is done, so the test waits for that to change instead of guessing a sleep;
the counting and preview passes leave last_sync alone (Synchronize.md#preview), and for them
a short sleep has to do. destroy() joins the thread either way.

## round-trip

The file lands in `<config>/autofilesync/state/`, the second run acts on the deletion the
first run recorded, the item's LastRun carries the count the assistant reads, and a run that
changes nothing leaves the file's write time alone (SyncState.md#changed).

## touches-nothing

Neither the counting pass nor the preview may leave a state behind: the counting pass does not
load one, the preview loads it to plan with but must not write it, or a later run would take
the preview's view of the sides for an agreed one.

## stopped-run

All 200 files are edited in the source, so the second run has 200 copies to make, and it is
stopped as soon as the first edited copy has arrived. (Emptying the target instead would not
do: with the state that reads as 200 deletions in the target, and the run would carry them to
the source.)
Whether the stop lands early or after the last file, every one of the 200 entries must still be
in the state afterwards: the files the run reached were recorded again, the ones it did not
reach were kept because a stopped run never prunes (Synchronize.md#complete-pass). The number
of files copied before the stop is printed, so a run that never gets stopped is visible.

## mass-deletion

The approval travels from Synchronize::create() into the pass (Synchronize.md#mass-deletion-approved):
without it the run refuses and records the refusal as its one error, with it the same situation
is carried out.

## one-way-mass-deletion

A one-way item that deletes in the target has no state between runs, but each run keeps a
notebook of its deletions (Synchronize.md#pass-only-state): a single deletion still goes through,
an emptied source does not empty the target, the refusal counts as an error, nothing is written
to the state directory and LastRun claims no tracked files; the run started from the preview
carries out the deletions.

## one-way-no-renames

The notebook must not switch on the rename detection: a new file that equals a kept one in
size and time is a copy, not a move, when the kept one is still in the source.
