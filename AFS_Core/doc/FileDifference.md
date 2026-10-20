# FileDifference

Module comment for FileDifference.h / FileDifference.cpp.

## ssh-error-is-not-absence

Connection/auth failure, not a confirmed-absent file: must not be mistaken for "source deleted",
or SyncFile deletes the destination (Fehler 2) the moment delete_in_target() is enabled.

## unreachable-is-a-warning

A host that cannot be reached is an expected state, not an error - see
SftpConnection.md#SshUnreachable. It counts as "drive not mounted" like every other failure here.

## ssh-content-fallback

A content comparison needs both files locally, so it cannot run for an SSH path. Skipping it left
an item whose only criterion is the content - the default for a new item - with nothing to
compare at all: every file that exists on both sides came out Equal, so after the first copy no
change ever reached the other side again. The content criterion therefore degrades to the size
for SSH paths, once in the constructor, which keeps compare_metadata() and the content check
consistent and makes compare_content() unreachable for SSH.

Size and time both, since the transfer carries the modification time of the file it copies (see
SyncFileSsh.md#transfer-keeps-the-modification-time) - a remote copy no longer looks newer than
the original it came from, so the time is the criterion that also notices an edit which leaves
the size unchanged.

## remote-time-conversion

file_time_type does not count from the Unix epoch - libstdc++ places its epoch in the 22nd
century, the Windows one in 1601 - so building one directly from the seconds an SFTP server
reports produced a time roughly two centuries off. Every remote file therefore compared as newer
than every local file, whatever its real age. util::to_file_time() converts through the clocks
instead.

## time-tolerance

A copy carries the modification time of its original, but only as precisely as the target file
system stores it. Compared to the nanoseconds of ext4, NTFS keeps 100 ns steps, exFAT 10 ms,
FAT32 whole 2 s, and HFS+ whole seconds; a copy therefore sits up to that step before its
original. Compared exactly, the original was newer on every pass and was sent again every time,
a backup of a source tree to an NTFS disk sent thousands of unchanged files a minute. Two write
times are therefore the same when they differ by no more than the item's tolerance - in both
directions, so that a two-way sync does not read the rounding as an edit on the other side
either. The default of two seconds covers every file system named above; a tolerance of 0 is
the exact comparison again. The price is that an edit less than the tolerance after the
previous write time goes unnoticed if a pass ran in between, which is why the size criterion
is worth adding where files are edited in quick succession.

## second-resolution

SFTP reports the modification time in whole seconds while a local file carries sub-second
precision, so a file and its own copy differ by up to one second. Comparing the seconds is what
keeps that difference from looking like a change even with a tolerance of 0; the tolerance is
then applied on top, in whole seconds.

## content-conflict

Two-way sync copies the newer file over the older one. When time and size are
equal but the content differs, neither side is the newer one, and copying in
either direction would destroy an edit. The result is therefore its own value,
which SyncFile leaves untouched and reports as an error.

## three-way

With a FileState from the last pass (SyncState.md#SyncState) the comparison has a third point
of view and four more answers. A side that is missing now but was there last time was deleted
there - SourceDeleted or DestDeleted - provided the other side is unchanged() since then; if
the other side was edited in the meantime, the edit wins and the plain SourceNotExits or
DestNotExists stands, which SyncFile copies as before and warns about. Both sides present:
the side that differs from the state is the one that changed, whatever the two write times say
about each other, so an edit that leaves an older write time (a restore from an archive, a
`touch` on the other side) is still propagated; both changed is BothModified, which nothing
copies. NeitherExists is the file that is gone on both sides: nothing to do, and the state
entry is dropped. Without a state nothing changes - the first pass after an update compares
the two sides as before.

## unchanged

A side is unchanged since the state when its size is the state's and its write time is the
state's within the item's tolerance (time-tolerance above), whatever the comparison criteria
say: the question is whether anybody touched this side, not how the two sides are compared,
and a deletion must not rest on a criterion the user switched off.

## both-changed-alike

Both sides differ from the state but not from each other - the same file dropped into both
directories, say - which is no conflict: with the same size and write time (within the
tolerance, whatever the criteria say - an item comparing by content alone would otherwise
call every pair of edits alike) the result is Equal, the content check runs if the item asks
for it, and the state is updated.

## absent

Only a file the file system (or the SFTP server) confirms as missing may count as deleted; a
file whose attributes cannot be read is still there and merely yields the old DestNotExists
or SourceNotExits, as before, which copies instead of deleting.

## observe_file

Size and write time of one side as read_file_info() reads them for the comparison, so the
FileState SyncFile records after a copy is measured the same way it is compared with next
time - an SFTP time in whole seconds included.

## base-only-two-way

A one-way item may carry a state too, for the rename detection (SyncItem.md#track_moves_),
but its comparison stays the plain one: the source is the master, a target the user deleted
is copied again, a target they edited is handled by copy_when_target_newer as before, and
nothing in the source is ever deleted. The state only answers where a new file came from.
