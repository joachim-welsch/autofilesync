# SyncBase

Module comment for SyncBase.h / SyncBase.cpp.

## sync_item_

Owned copy, not a reference: sync operations can run long enough (large file copies) that a live
reference into Config's syncitems_ vector could be invalidated by a concurrent GUI-thread
add()/remove()/erase().

## sync_file

Synchronises one file of the item; only counts it when only_count_ is set, without comparing,
unless a preview collects a plan (SyncPlan.md#planning) - then SyncFile compares and records
the decision. Every failure is logged and reported here, so one bad file never stops the
directory it lives in.

## sync_target_only_entries

Files that exist only in the target directory (one level, not recursive) never reach
FileDifference at all otherwise: SyncDir/SyncTree only ever enumerate the source side, so a file
removed from source (delete_in_target) or one that appeared only in the target (two_way_sync)
was never even looked at. sourceNames is every filename seen in this same directory level's
source-side pass (matched by wildcard or not - either way it is not "missing"), so this only
acts on names absent from it. Reuses SyncFile's existing FileDifference-based decision (delete
vs. copy back) by constructing it with a source path that is confirmed not to exist.

## father_son_backup

The father-son principle keeps every replaced target as "<name>.bak.<n>" (SyncFile's
move_file_to_backup, which shares FATHER_SON_SUFFIX). Those generations exist only in the
target, so sync_target_only_entries would otherwise treat them like any file the source no
longer has: delete_in_target removed them in the same pass that created them, and two_way_sync
copied them back into the source. They are skipped only while the item keeps generations; an item
without the principle still handles a stray ".bak.1" like every other target-only file.

## SyncException

what() carries the UTF-8 encoded message, so the catch(std::exception&) handlers see the same
diagnostic as get_message().

## state_

The sync state of the item's last pass (SyncState.md#SyncState), a state for this pass only
for a one-way item that deletes in the target (Synchronize.md#pass-only-state), or null for
every item that has none - any other one-way item, a counting pass. Not owned: Synchronize keeps it for the duration of
the pass and writes it afterwards; every SyncFile the pass creates gets the same pointer, so
the entries one file records are there when the next file is compared.

## state_key

The entries of the state are keyed by the file's path relative to the item's source
(SyncState.md#path). The root is taken once, as SyncFile::operator()() sees the paths it is
given (without the volume specification, with the account prefix for SSH), and passed down
unchanged to the SyncFiles of a directory or tree, whose own sync_item_ names a single file.
A file item has no path below its root, so its key is the file name.

## side_path

The full path of a state entry on one side: the key is relative to the item's roots with "/"
between its parts (SyncState.md#path), so it gets the root of the side it is wanted on and that
side's separator - "/" for an SSH side, the platform's own for a local one. A rename uses it to
look at the old name on both sides (SyncFile.md#renames).

## finish_pass

The end of a pass, run once by the SyncDir or SyncTree that started it, after the whole tree
was compared; without a state there is nothing to finish. With one: first the deletions the
pass put off (SyncFile.md#deferred-deletions), all of them or - after a refused mass deletion -
none, then the directories that were to be removed once empty (SyncTree.md#prepare-and-settle).
The directories come last because they are only empty after the deletions; when the deletions
were refused, they stay as well. A stop request ends the loop between two files, and what was
not deleted is found again by the next pass.

## create_source_dir

The counterpart of create_target_dir_if_not_exist() for a directory that exists only in the
target of a two-way item (SyncTree.md#target-only-directories): it is new there and gets its
mirror in the source, with every missing level. Like the target side, nothing is created on a
counting pass.

## remove_dir_if_empty

A directory the other side deleted is emptied file by file (SyncTree.md#prepare-and-settle),
and only then, if nothing is left, removed - a file that survived because it was edited, or a
".bak" generation, keeps the directory on both sides. An empty directory holds nothing to
recover, so it is removed outright rather than through the recycle bin; a non-empty one is
simply left, without a message, because that is the ordinary outcome.

## mass-deletion

A target that was emptied or swapped - a freshly formatted backup disk, a drive letter that
now points elsewhere - reads to the three-way comparison as "everything deleted over there",
and a pass that believed it would empty the other side. So a pass that would delete more than
MASS_DELETION_PERCENT of the files the state knows, and at least MASS_DELETION_MIN_FILES of
them (deleting three of four files is everyday work, not a disaster), refuses all of its
deletions, leaves the state as it is so that the next pass sees the same situation, and says
why: once per item at error level, into the log, the proof and the pass's notes
(Synchronize.md#notes), which reach the user past the message filter, until a pass goes
through normally. The way out is deliberate: the preview still plans the deletions and warns in its
notes, and the run started from the preview carries the pass's approval
(SyncState.md#mass-deletion-approval), because the user has then seen the list. The daemon
can never approve; it keeps refusing until the user has looked.

## known-files

The files the state knows plus the deletions it does not know. For a kept state every
deletion is one of its entries, so the sum is just its size; a pass-only state knows only the
files this pass found in place or copied (SyncFile.md#state), never the ones it is about to
delete, so those are added - otherwise twelve deletions next to twelve kept files would count
as twelve out of twelve.
