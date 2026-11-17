# SyncLink

Module comment for SyncLink.h / SyncLink.cpp.

## SyncLink

Carries one symbolic link to the other side as a link, for an item whose links are to be kept
as links (SyncItem.md#symlink_mode_). It is the counterpart of SyncFile for an entry that has
no content, no size and no time worth comparing: the only thing two links can differ in is
their text. The source is the master; the state, the way back and deletions are not handled
here yet.

The text that is compared and written is the portable one (SymlinkUtil.md#portable_link_text):
an absolute link into the synchronised tree becomes relative, so the link in the target leads
to the copy and not back into the source, and the link in the source is never rewritten. The
target's link is read through the same rule with the target's root, so a pair that was brought
in line stays equal. A link that leads nowhere is carried over like any other - whether it
leads somewhere is the user's business, and it may well do so on the other side.

## local-only

Both sides have to be local. A link whose source or target lies behind SSH or in a cloud is
counted as "not supported there" and left alone; the content is never copied in its place,
since a file that came back from such a target would replace the link in a two-way item.

## blocked-by-directory

A real directory in the target under the link's name is never removed to make room: it may
hold files that exist nowhere else. It is an error with the directory's name, and the link is
tried again in the next pass.

## create

The link is first created beside its place, under "<name>.afs-link", and read back; only when
the text is what was asked for is the old target touched. A file system without links refuses
at this first step, so on a FAT drive or without the privilege on Windows nothing in the
target is renamed, pass after pass. The read-back also catches a file system or server that
takes the request and stores something else. A leftover of that name from a run that was
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

## LinkTally

What a pass has to say about its links once, not per link: those the target could not take,
those of the system that were skipped (SymlinkUtil.md#system_link), those that point out of the
synchronised tree and were copied as they are, and the links created inside the tree, which
are asked at the end of the pass whether they lead somewhere - only then are the files they
point to copied as well. Each group keeps its number and its first link for the message
(SyncBase.md#report_links). The pass owns the tally; every SyncLink of the pass adds to it.
