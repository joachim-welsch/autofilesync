# SyncTreeTests

Module comment for SyncTreeTests.h / SyncTreeTests.cpp.

## SyncTreeSymlinkCycleDoesNotCrash

Regression test for Fehler 3: SyncTree::sync_branch() recursed via
fs::directory_entry::is_directory(), which follows symlinks by default, with no cycle detection.
On Linux the kernel's own ELOOP guard eventually aborts a self-referencing symlink chain
(confirmed: ~40 levels, ~80 wasted directories physically created in the target) before the
process crashes from stack exhaustion - so a plain "does it crash" check is not reliable across
platforms/filesystems. The deterministic, portable assertion is that a symlinked directory must
never be descended into at all.

## symlink-privilege

Windows needs SeCreateSymbolicLinkPrivilege for symlinks, so either developer mode or an
elevated session. Without either the precondition cannot be established - skip instead of fail.

## DeleteInTargetRemovesFileMissingFromSourceAtEveryLevel

Fehler 1 (round 7): like SyncDir, SyncTree only ever enumerated the source side at every
recursion level, so delete_in_target/two_way_sync had no code path for Tree items either.
Verifies both the top level and a *nested* subdirectory level correctly detect a target-only
file (proving the fix applies per recursion level, not just once at the top).

## ExcludedDirectoryIsNeverEnteredNotJustFilteredFileByFile

Exclusions feature: an excluded *directory* must be skipped entirely (never even entered), not
just filtered file-by-file - the whole point of excluding e.g. "node_modules" or ".git" as a
subtree, matching how the symlink-loop protection above already treats directories specially
rather than relying on per-file filtering.

## DeleteInTargetKeepsTargetWhenSourceDirIsGone

See SyncDirTests.DeleteInTargetKeepsTargetWhenSourceDirIsGone - for a tree the stakes are higher
still, since a whole backup hierarchy hangs off the target. An unavailable source root must
leave every level untouched.

## DeleteInTargetLeavesTargetOnlyDirectoriesAlone

A source subdirectory that disappears while the source root still exists: only that branch's
target counterpart may be affected, and since a one-way item never descends into a target-only
directory, its files stay. Pins down the deliberate boundary of the option - deletions are
mirrored per file, not for whole directories; only a two-way item with a state goes further
(TwoWaySyncTests.md#deleted-directories).

## SyncTreeUnreachableSshSourceTest

See the identical fixture in SyncDirTests.cpp / FileDifferenceTests.cpp: ".invalid" can never
resolve, so every SSH call fails fast without needing a real server.

## DeleteInTargetKeepsTargetWhenSshSourceIsUnreachable

ssh_list_dir() cannot distinguish "connection failed" from "empty directory", so the target-only
pass runs over the whole target at every recursion level. FileDifference's per-file check is
what keeps the files - assert it holds for the nested level too, where a tree has most of its
data.
