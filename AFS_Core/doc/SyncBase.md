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
