# SyncTree

## target-only-files-at-this-level

Target-only subdirectories are intentionally out of scope here (see sync_target_only_entries()'s
doc comment) - only files at this level.

## sync_entries

Returns every name seen in this directory level's source listing, so the target-only pass can
tell "not in source" from "wildcard-excluded, ignore" - see
SyncBase::sync_target_only_entries(). Subdirectory names are included, so a target file sharing
a source subdirectory's name is never mistaken for an orphan.

## symlinks-never-followed

Never follow a symlink to a directory: a link back to an ancestor would recurse forever. An SSH
listing cannot tell what a link points to without an extra round trip, so there every symlink is
skipped.

## excluded-directories

An excluded directory is skipped as a whole, not filtered per contained file: a subtree like
"node_modules" is never entered.
