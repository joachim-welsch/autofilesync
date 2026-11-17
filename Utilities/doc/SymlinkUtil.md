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
