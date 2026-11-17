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

A target directory that cannot be listed ends the work on this directory with one error. It
used to pass for an empty one without a word: its files were then asked for one by one, which
kept them from being deleted, but a directory new in the target went unseen. A target that is
simply not there is no error - a preview has not created it.

## prepare-and-settle

prepare() creates the side a new directory lacks before the files are compared, and settle()
removes the side a deleted directory leaves empty after they are - only after, so that a file
edited since the deletion keeps its directory on both sides (SyncFile.md#parent-of-a-kept-file),
and never on a counting pass or a preview, which touch nothing. A stopped pass does not settle
either: a directory whose files were not all visited is not known to be empty. When prepare()
cannot create the directory, this directory and everything below it is given up
(SyncBase.md#create_target_dir_if_not_exist).

## sync_entries

Returns every name seen in this directory level's source listing, so the target-only passes can
tell "not in source" from "wildcard-excluded, ignore" - see
SyncBase::sync_target_only_entries(). Subdirectory names are included, so a target file or
directory sharing a source subdirectory's name is never mistaken for an orphan.

## symlinks-never-followed

The default mode (SyncItem.md#symlink_mode_). Never follow a symlink to a directory: a link back
to an ancestor would recurse forever. An SSH listing cannot tell what a link points to without
an extra round trip, so there every symlink is skipped.

## list_source

The questions about an entry are asked without exceptions: a link that runs in a circle makes
the throwing ones fail, and the entry was then dropped with an error although it is just a
link that is neither a file nor a directory. It now takes the same way as a dangling link.

## links-as-links

With the mode "as link" every symbolic link of a local source - to a file, to a directory, to
nothing - is handed to SyncLink (SyncBase.md#sync_link) instead of being followed or skipped.
An excluded name is left out like any other; the wildcard applies to a link to a file, not to
one to a directory, as it does not apply to directories.

## excluded-directories

An excluded directory is skipped as a whole, not filtered per contained file: a subtree like
"node_modules" is never entered.

## followed-links

With the mode "follow the link" (SyncBase.md#follows_links) a link to a directory is entered
like a directory: its content is copied under the link's name, and the target gets a real
directory. A link to a file is copied as its content, as in the default mode. The walk keeps
a LinkTrail (SymlinkUtil.md#LinkTrail) of the directories it is in, and asks it before every
link: a link back onto the way, one into or above the target tree and one behind too many
others are not entered, and neither is one that leads nowhere. Each kind is counted and named
once at the end of the pass (SyncBase.md#report_links) and is no error - a loop in a tree is
something the user may well have on purpose. Directories entered by name are told to the
trail as well, because a link further down may point back to them. An excluded link is not
entered, and a link in the target where the followed directory belongs is not written through
(SyncBase.md#target_usable). A directory item does not descend, with or without a link.
With the source behind SFTP the server is asked: what a link leads to with the one call that
also tells a dead link, and for a directory where it really lies (SyncFileSsh.md#real-path) -
one more round trip per directory link and one for the root. The trail then works on the
server's answers (SymlinkUtil.md#LinkTrail-resolved). A link the server gives no answer for is
left alone; without the root's real path nothing is followed in that pass. The target tree
counts for the guard only when it lies on the same server.

## junctions

Windows only (SymlinkUtil.md#junction). To std::filesystem a junction is a directory, and the
default mode goes on entering it as one: an item that reached a moved folder through a
junction must not lose it with an update. Only a junction of the system is taken out there,
like any link of the system. In the two new modes a junction counts as a link: the mode that
keeps links does not recreate it and names the skipped junctions once per pass, and the mode
that follows links enters it under the guard against loops, which a plain directory does not
get. Written for Windows and never compiled or run there.
