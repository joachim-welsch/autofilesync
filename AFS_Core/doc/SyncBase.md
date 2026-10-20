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

The sync state of the item's last pass (SyncState.md#SyncState), or null for every item that
has none - a one-way item, a counting pass. Not owned: Synchronize keeps it for the duration of
the pass and writes it afterwards; every SyncFile the pass creates gets the same pointer, so
the entries one file records are there when the next file is compared.

## state_key

The entries of the state are keyed by the file's path relative to the item's source
(SyncState.md#path). The root is taken once, as SyncFile::operator()() sees the paths it is
given (without the volume specification, with the account prefix for SSH), and passed down
unchanged to the SyncFiles of a directory or tree, whose own sync_item_ names a single file.
A file item has no path below its root, so its key is the file name.

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
