# SyncPlanTests

Module comment for SyncPlanTests.cpp.

## preview

Runs an item the way Synchronize runs a preview pass (Synchronize.md#preview): counting
mode under a SyncPlanGuard. The fixture works in its own directory below the scratch
directory and removes it afterwards, so it leaves nothing behind whatever a test asserts.

## PreviewListsCopyAndDeletionAndTouchesNothing

The whole point of the preview in one test: a tree with a new file, a changed file in a
subdirectory and a file that exists only in the target, with delete_in_target on. The plan
has to name all three with the right action, and the directory listing and the old target
content have to be exactly as before. The count of the pass equals the plan's count so the
progress bar of a run started from the preview gets the same range.

## EmptySourceFileIsNotPlannedUnlessWanted

The run skips an empty source file unless sync_empty_files is set
(SyncFile.md#empty-source-file); the preview must show the same, or the user would see a copy
that never happens.

## CountingWithoutAPlanStillCountsEveryFile

The plain counting pass of "Einmal synchronisieren" still counts every file it looks at, not
only the differing ones (SyncOnceTest.md#nothing-to-do); only the preview pays for the
comparison. Pinned down so a change there is a conscious one.

## ListStopsAtMaxStepsWhileTheCountsGoOn

The cap of SyncPlan.md#MAX_STEPS is tested on the plan alone: filling ten thousand files on
disk would slow the suite for nothing.

## PreviewPassNamesTheUnreachableItems

The one case the fixture's preview() cannot reach, because the reachability check sits in
Synchronize::run_sync_item: a whole Synchronize pass with one item whose target does not
exist and one whose sides match, read through the test-only unreachable() accessor because
the hand-off to the controller is compiled for the GUI only. The list names the item that was
never compared (SyncPlan.md#UnreachableItem), and only that one, with the warning on by
default and the log line that carries the path. The second test turns the warning off and
checks that the run records the item as unreachable but without an error
(SyncItem.md#warn_if_unreachable_).
