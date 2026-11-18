# SyncLink

Module comment for SyncLink.h / SyncLink.cpp.

## SyncLink

Carries one symbolic link to the other side as a link, for an item whose links are to be kept
as links (SyncItem.md#symlink_mode_). It is the counterpart of SyncFile for an entry that has
no content, no size and no time worth comparing: the only thing two links can differ in is
their text (decide).

The text that is compared and written is the portable one (SymlinkUtil.md#portable_link_text):
an absolute link into the synchronised tree becomes relative, so the link in the target leads
to the copy and not back into the source, and the link in the source is never rewritten. The
target's link is read through the same rule with the target's root, so a pair that was brought
in line stays equal. A link that leads nowhere is carried over like any other - whether it
leads somewhere is the user's business, and it may well do so on the other side.

## decide

What is done with a pair, from what lies on each side under the name - a link with its portable
text, something else, or nothing - and, for a two-way item, the text the state remembers
(SyncState.md#links). Two links with the same text are settled: the state records the text.
One-way, the source's link goes to the target whatever is there, and a link only the target
has is left alone unless "delete in target" is set. Two-way, the state tells a change from the
other side's change and a missing link from a deleted one: the side that still has the
remembered text is the one that did not change, so the other side's text is carried to it, or
the other side's deletion repeated on it. Without a remembered text a link that only one side
has is new and is created on the other - never deleted, since nothing says it was ever there.
Two links that both differ from the remembered text, or differ with nothing remembered, and a
link that meets a file in a two-way item, are a conflict. A name that holds something else
than a link in the source is not this module's business; the file pass handles it
(SyncBase.md#target_usable).

## remove

A deletion is put off to the end of the pass like a file's when the item keeps a state
(SyncFile.md#deferred-deletions), with zero bytes: it then counts for the guard against mass
deletions and is carried out by the same code, which removes the link itself and never what it
points to. Without a state it is carried out or planned at once.

## conflict

With conflict copies the target's entry is renamed to the conflict name a file would get
(SyncFile.md#conflict-copy-name) and the source's link takes the name; the renamed entry is
then new in the target and travels to the source with the next pass, like any new link or
file. A link has no time that could say which side is newer, so the source wins the name by
rule, and nothing is lost either way. A preview plans it as a conflict copy into the target. Without conflict copies both sides stay as they are and
the pair is an error of every run until the user settles it; the log gets the line once.

## places

Each side of the pair is a place: its path, the root of its tree and whether it lies behind
SFTP. A local place is asked through SymlinkUtil, a remote one through the link calls of the
SSH layer (SyncFileSsh.md#links); renaming, removing and creating go the same two ways. The
rules about a link's text work on the path as the side itself sees it, so for a remote place
the account prefix is taken off first. A link's text is a path of the machine it lies on: a
relative one means the same on both sides, an absolute one that leaves the tree is copied as
it is and reported like any link that leaves the tree.

A side that could not be asked is not a side without a link. Nothing is decided for the pair
then, since "nothing there" would create or delete (unasked). Where
only a free name is looked for, no answer counts as free: the rename that follows fails and
is reported, while "taken" would search without end.

## unasked

No answer about a side - the server did not reply or refused the question, a local directory
may be listed but not searched (SymlinkUtil.md#link_info-refusal) - ends the work on the pair
with an error that names the side and the reason. The local side used to take "no answer"
for "no link": a two-way item then deleted the source link whose target link it could not
see, and a mirroring item deleted the target link whose source link it could not see. The
files of such a directory were always refused (SyncFile.md#not-readable); the links now are
too. A server that gave no answer said nothing before, not even in the log, and the link
stayed unsynchronized without a trace. A pass that only counts reports nothing, as elsewhere.

## no-links-there

A side in a cloud has no symbolic links at all. A link whose source or target lies there is
counted as "not supported there" and left alone; the content is never copied in its place,
since a file that came back from such a target would replace the link in a two-way item.

## move

"Delete after copying" removes the source link once the target link is in place, but only a
local one, as for files: nothing behind SSH is deleted for a move. A source link that would
not go is reported like a file's (SyncFile.md#move-keeps-the-copy), and a source link whose
target link says the same is removed by the next pass (SyncBase.md#completes_moves).

## blocked-by-directory

A real directory in the target under the link's name is never removed to make room: it may
hold files that exist nowhere else. It is an error with the directory's name, and the link is
tried again in the next pass.

## create

The link is first created beside its place, under "<name>.afs-link", and read back; only when
the text is what was asked for is the old target touched. A file system without links refuses
at this first step, so on a FAT drive or without the privilege on Windows nothing in the
target is renamed, pass after pass. The read-back also catches a file system or server that
takes the request and stores something else, which is a SymbolicLinkError: the place took a
link, only not this one. A leftover of that name from a run that was
killed is the sync's own and is removed first. Then the old target is parked, the prepared
link renamed into place and the parked target discarded; if the rename fails, the parked
target comes back.

A refusal because links are not supported is no error: it goes into the tally and is reported
once for the whole pass. Every other refusal is an error with the link's name and the reason.

## park

What the link replaces - a file or another link - is renamed, not deleted, until the new link
is in place: to the first free "<name>.bak", ".bak1", ... which is removed afterwards, or with
the generation principle to the next "<name>.bak.<n>", which stays as with a replaced file
(SyncFile.md#move_file_to_backup). "Free" asks for the name itself, so a dangling link of that
name counts as taken.

## stranded

When the prepared link cannot be renamed into place, the parked target is renamed back. If
that fails as well, the place is empty and what lay there has another name. The file is not
lost, but a user who only reads "link could not be created" looks for it in vain: so the
error then says under which name the previous target now lies. The outcome of the way back
used to be ignored.

## LinkTally

What a pass has to say about its links once, not per link: those the target could not take,
those of the system that were skipped (SymlinkUtil.md#system_link), those that point out of the
synchronised tree and were copied as they are, and the links created inside the tree, which
are asked at the end of the pass whether they lead somewhere - only then are the files they
point to copied as well. The mode that follows links adds the links it did not enter
(SyncTree.md#followed-links). Each group keeps its number and its first link for the message
(SyncBase.md#report_links). The pass owns the tally; every SyncLink of the pass adds to it.
