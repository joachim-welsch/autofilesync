# SymlinkUtil

Module comment for SymlinkUtil.h / SymlinkUtil.cpp.

What the synchronisation has to know about a symbolic link before it touches it. Nothing here
knows about synchronisation items; the rules that depend on them live in AFS_Core.

## dangling_link

Whether a path is a symbolic link that does not lead to anything: its target is missing, or
the chain of links runs in a circle, which the system answers with "too many levels of
symbolic links". Everything that follows links reads such a path as a missing file, and a
file opened for writing is created where the link points. A caller that would take "missing"
for "deleted", or that is about to write, has to ask this first (SyncFile.md#dangling-links).
A path that is no link, or a link that resolves, is not dangling.

## link_info

What a path is as a link: whether it is one, the text it stores - not what that resolves to -,
whether it leads to something and whether that is a directory. The last decides which kind of
link Windows needs, and it is false for a link that leads nowhere.

## lexical

The rules about a link's text work on the text alone: dots are folded, a trailing separator
is dropped, and one path lies within another when it begins with all of its components -
"/daten/quelle2" does not lie within "/daten/quelle". Nothing is resolved, because the text of
a link is what gets written to the other side, and the other side has other links.

## portable_link_text

The text a link gets on the other side. Relative text is kept as it is. Absolute text that
points into the synchronised tree is made relative to the link's own directory: the absolute
text would point back into the source from the target, or to nothing after a restore on
another machine, while the relative one leads to the copy. Absolute text that points out of
the tree stays, since there is nothing on the other side it could be bent to (leaves_root).

## leaves_root

Whether a link points out of the synchronised tree, by its text. Such a link is copied as it
is and may lead nowhere on the other side, so the caller tells the user.

## system_link

A link the system made for itself and that means nothing on another drive: one that resolves
into /proc, /sys, /dev, /run or /snap on Linux, into /System, /dev or /private/var/db on macOS,
or to a device, a pipe or a socket anywhere. Below /run the removable drives (/run/media,
/run/mount) and the user's own directory (/run/user) are the user's and do not count. A link
that leads nowhere is not a system link; it is dangling. Windows has no rule here yet.

## create_link_refusal

Creates a link with the given text and says why when that fails. LinksNotSupported is the
answer of a file system that has no links - FAT and exFAT say "operation not permitted" - and
of Windows when the user lacks the privilege to create them; the caller reports that once per
run and not for every link. Windows needs to know whether the link leads to a directory. As
with the file time, a failure cannot be provoked on a file system that has links, so tests set
the error (FilesystemUtil.md#write_time_refusal).

## LinkTrail

The guard against endless descent when directory links are followed. It keeps the physical
directories of the way down from the root to where the walk stands - not everything visited,
so two links to the same directory beside each other are both followed. A link is refused
when it does not lead to a directory (Unresolved), when its target is one of the directories
on the way or contains one (Loop: entering it would come back here), when it lies in or above
the target tree (IntoTarget: what is copied would be read again, without end), or when the way
already leads through MAX_FOLLOWED_LINKS links (TooManyLinks: no real tree is that indirect,
and a chain that long is more likely a trap than a wish). The loop is asked first, since a
link above both trees is foremost a loop. Directories entered by name cost no call to the
system; only a link is resolved. A target that is not on this machine is given as none; a link
cannot lead into it.
