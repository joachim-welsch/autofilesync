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

## link_info-refusal

An entry that cannot be asked is not an entry that is not there. A directory that may be
listed but not searched names its entries and refuses every question about them; the system
then gives no file type at all, and that - not the error code, which a missing entry sets as
well - is what is taken for "no answer". The reason goes back in refusal, and nothing else
in the answer means anything then. A link whose text cannot be read is refused the same way
instead of being handed on with an empty text. The caller must not decide anything for such
an entry (SyncLink.md#unasked).

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
that leads nowhere is not a system link; it is dangling. On Windows it is a symbolic link or a
junction that bears the "system" or "hidden" attribute - the compatibility junctions such as
"Documents and Settings", which nobody may list - and an app execution alias, which is no file
that could be read. The Windows rule is written and has never been compiled or run.

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

## LinkTrail-resolved

The same guard for a tree this machine cannot resolve, such as one behind SFTP: the caller
asks the other side where a root or a link really lies and hands the answers in. of_resolved()
starts a trail from roots that are already resolved, refusal_of() judges a directory a link
leads to, and enter_resolved() enters it. Nothing here touches the file system. The local
refusal() and enter_link() resolve and then do the same.

## rooted

Whether a link's text names a fixed place rather than one relative to the link. On Unix that
is an absolute path. On Windows std::filesystem calls "/srv/daten" relative, because it has no
drive; but the text of a link on a server is a path of that server, and there it is absolute.
So a text with a root directory counts as rooted on every platform. On Unix nothing changes.

## junction

A Windows junction: a reparse point of the mount-point kind, which leads to another directory
like a symbolic link but is none to std::filesystem, which calls it a directory. The default
mode goes on treating it as the directory it leads to, as it always did. The mode that keeps
links as links does not recreate it - a junction holds an absolute path of this machine and
cannot be created on a server or in a cloud - and the mode that follows links enters it with
the guard against loops (SyncTree.md#junctions). A mounted volume in a folder is the same kind
of reparse point and is treated alike. Always false elsewhere. Written for Windows and never
compiled or run there.
