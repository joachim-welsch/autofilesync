# SyncTree

## target-only-files-at-this-level

sync_target_only_entries() handles the files of this level only; the directories that exist
only in the target are the tree's own business (target-only-directories below). The call is
skipped where the target directory does not exist - it was just created empty, or it is a
directory the target side deleted.

## DirState

Each directory of the tree is in one of five states, decided by its parent from what both
sides list: present on both sides; new in the source (no state knows a file below it, the
ordinary case of a one-way item too); new in the target; deleted in the target (the source
still has it, the state knows files below it, the target does not have it); deleted in the
source (the mirror image). The state only ever knows files, so a directory that was empty
last time counts as new (SyncState.md#knows_below). A one-way item never lists the target
and has no state, so its subdirectories are all treated as present on both sides, which is
the path they always took: the target directory is created if it is missing and the
target-only files of the level are handled.

## target-only-directories

A two-way item lists the target's subdirectories once per level, next to the source listing.
A directory the source does not have is entered like any other, with the source side missing:
new in the target, it is mirrored into the source and its files come back through the
ordinary target-only pass; deleted in the source, its files meet the three-way comparison
one by one (FileDifference.md#three-way) - the unchanged ones are deleted, an edited one is
copied back - and what is left empty is removed. The same per-file decision serves a
directory the target side deleted, from the source listing. The second listing costs one
more round trip per directory on an SSH target; a one-way item pays nothing.

## prepare-and-settle

prepare() creates the side a new directory lacks before the files are compared, and settle()
removes the side a deleted directory leaves empty after they are - only after, so that a file
edited since the deletion keeps its directory on both sides (SyncFile.md#parent-of-a-kept-file),
and never on a counting pass or a preview, which touch nothing. A stopped pass does not settle
either: a directory whose files were not all visited is not known to be empty.

## sync_entries

Returns every name seen in this directory level's source listing, so the target-only passes can
tell "not in source" from "wildcard-excluded, ignore" - see
SyncBase::sync_target_only_entries(). Subdirectory names are included, so a target file or
directory sharing a source subdirectory's name is never mistaken for an orphan.

## symlinks-never-followed

Never follow a symlink to a directory: a link back to an ancestor would recurse forever. An SSH
listing cannot tell what a link points to without an extra round trip, so there every symlink is
skipped.

## excluded-directories

An excluded directory is skipped as a whole, not filtered per contained file: a subtree like
"node_modules" is never entered.
