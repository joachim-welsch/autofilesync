# SyncFile

Module comment for SyncFile.h / SyncFile.cpp.

## wait_for_updated_filetime_call_count

Counts calls to the free wait_for_updated_filetime() below, so a regression test can catch a
caller waiting for the same filetime confirmation twice.

## bounded-retries

Bounded retries: transient errors right after a copy settle quickly, but a persistent one
(target drive unplugged) must not spin forever - the GUI's stop_sync() busy-waits for this
thread to finish.

## restore_backup

Removes a failed or aborted copy and puts the previous target back. The partial target lives
where the copy wrote it: a local remove_file() on an "account:/path" string touches nothing, so
an SSH target is removed through SFTP instead - otherwise a cancelled upload left the half file
on the server (SyncSshStopTest). The exists check first keeps a transfer whose remote open
already failed from logging a second error for a file that was never created. There is no remote
backup yet: an existing remote target is truncated by the upload itself, so only the half file
can be cleared, not the old content restored.

## backup-rename-failure

The rename of the old target to ".bak" is the first step of the copy, and a target directory
without write permission refuses it. rename_file() logs that, but the item was only reported
as failed when the copy itself failed - so the proof file and the last-run record claimed a
clean run while the target stayed old. It is reported with its reason and the target's name
(refused): on Windows the usual one is a target another program has open.

## free_backup_name

The old target is parked as "<name>.bak" while it is replaced. A file of that name that is
already there is not the sync's to delete: it may be the user's own backup, or the only good
version left over from a run that was killed mid-copy. So the first free name of ".bak",
".bak1", ".bak2" ... is taken, and discard_backup() and restore_backup() only ever touch the
file this copy parked itself.

## discard_backup

The counterpart for the successful copy: the ".bak" exists only so restore_backup() has
something to put back, so it must not outlive the copy it protected. A leftover would not just
waste the space of a second full copy - with two_way_sync it is a source-only file for the next
pass and gets copied to the other side, and the sync never settles.

## copy-direction-follows-the-paths

Which of the four copy functions applies follows from the two paths, not from the SyncItem:
copy_dest_to_source() passes them the other way round, so deciding by ssh_source()/ssh_target()
picked the opposite direction for every copy back from the target - e.g. copy_fs_to_ssh() with
the remote path as its local source, which can only fail. get_account_from_path() is the same
classification SyncItem uses when it builds its paths.

## copy_local_file

fs::copy_file copied in one call: nothing for the progress bar until the file was through, and
no way to stop inside a large file. The block loop reports the bytes after every block
(Synchronize.md#copied_bytes_) and checks the stop flag, so Cancel ends inside the file and
save_copy_file()'s Stopped path removes the partial target. The source's permission bits and
the file time follow separately (attribute-failure, file-time-failure).

## move_file_to_backup

The father-son principle keeps the replaced target as "<name>.bak.<n>" with the first free n,
so every generation survives (SyncBase.md#father_son_backup). first_free_generation() only
knows how to ask whether a name is taken; the local variant asks the filesystem, the remote one
(below) a directory listing fetched once.

## remote-father-son

A remote target is moved aside through SFTP: one listing of its directory answers both whether
the target exists and which generations are taken, so a file with many generations costs two
round trips instead of one stat per generation plus the rename. ssh_rename() refuses an existing
destination on most servers (SyncFileSsh.md#ssh_rename), which is why a free name is chosen from
the listing rather than relied upon. A failed rename stops the copy like the local variant: the
upload would otherwise overwrite the generation the user asked to keep. The name begins after the
last separator of either kind, because a Windows-side target path may still carry backslashes
that get_remote_path() translates later.

## pre_copy

Makes an existing target replaceable before the copy: the owner gets all rights so a read-only
target no longer refuses the backup rename (a failure is only logged, with the target's name:
if it matters, the rename fails next and is reported), and the father-son principle moves the old target
aside. The chmod is a local operation, so a remote target only gets the father-son backup here
(remote-father-son); its write bit is restored by the upload itself when the open is refused
(SyncFileSsh.md#read-only-target-made-writable). A download from an SSH source to a local target
used to skip both, which silently dropped the father-son generations the user had asked for.

## ssh-skips-verification-only

SSH skips only the content verification below (TODO); Error/Stopped must still run the
cleanup/restore paths, or a failed transfer is reported as success - the caller would then e.g.
delete the source (del_aftercopy).

## decide

One place for the decision what to do with a file - delete it in the target, copy or move it
there, copy it back, or leave it alone - so the counting pass, the preview and the real run
cannot drift apart. The real run carries the decision out, the counting pass counts it, the
preview records it (SyncPlan.md#SyncPlan).

## not-readable

A side that could not be read (FileDifference.md#not-readable) ends the work on this file
before anything is decided: nothing is copied, deleted or planned, and the state keeps what it
knows, so the pass after the cause is gone finds the file as it was. The file and the reason go
into the report, because a run that could not look at a file must not count as successful.

## refused

A file can have a size and a time and still not open: another program has locked it, or the
right to read it is missing. The copy used to find that out last - after the old target had
been renamed to ".bak" and the new one opened for writing - and then put everything back, with
"copy failed" as the only word to the user. Now the source is opened before anything on the
other side is touched - before pre_copy() as well, which with the generation principle moves
the old target to its backup name - and a refusal is reported with its reason and the file it belongs to
(FilesystemUtil.md#read_refusal). The same check stands before a conflict is resolved: the
losing file is renamed to its conflict name first, and with a winner that cannot be read the
losing side would be left without the file under its own name. Paths behind SSH or in a cloud
are not asked this way; their transfer reports its own failures.

## copy-failure-reason

Why a local copy failed, with the file the failure belongs to: the target that cannot be
opened for writing, the disk that is full, the source that stops reading. The stream only says
that it failed; the reason is the system's error at that moment
(FilesystemUtil.md#failure_text), and it is added to the "copy failed" report in brackets. The
transfers behind SSH and in a cloud log their own reasons and leave this empty. A write that
ran out of room is no "copy failed" but a shortage like the one found beforehand (disk-full).
A failure on the target is judged as a write (FilesystemUtil.md#reason_for_write_failure), one
on the source as a read.

## disk-full

Before a local copy the room on the receiving drive is asked for
(FilesystemUtil.md#free_space). A file larger than that is not tried: the try would rename the
old target to ".bak", write until the disk is full, remove the half file and put the old one
back - a lot of writing on a disk that has no room, for nothing. The whole file has to fit
next to the old target, because the old one stays until the new one is verified; that is the
price of getting it back when a copy fails. The pass goes on, since smaller files may still
fit and deletions make room, and the files that do not fit are summed up in one entry of the
report (CopyLog.md#shortage); each is a warning in the log. No answer about the room means
try: a quota or a drive that does not say shows itself in the write, which is reported the
same way.

## only-count-touches-nothing

only_count_ means "count only" - it must not touch the filesystem, the way
SyncBase::sync_dir_entry() already handles it for Dir/Tree items. A file the run would skip
as empty is neither counted nor planned, so the preview shows what the run does.

## empty-source-file

An empty file is more often an aborted write than an intentionally empty file; copying it over
an existing one would lose data. Placeholders like .gitkeep need sync_empty_files. A deletion
has no file to be empty; a copy back looks at the target's size, the file it would copy.

## volume-is-target-size

The target file is the one being copied here, so its size is the volume moved - filesize is the
source's (see FileDifference).

## ssh-target-delete

util::remove_file() interprets "account:/path" as a bogus local path and silently does nothing
for it. A remote file cannot go into the local recycle bin without downloading it first, so SSH
targets stay a permanent delete.

## failures-return-early

Every failure returns before the bookkeeping below: a copy that did not happen must neither
count as done nor let del_aftercopy delete the source.

## local-only-filetime

fs::last_write_time() only resolves a local path, so the time is stamped here only when both
sides are local. Every transfer with an SSH side brings the modification time along itself (see
SyncFileSsh.md#transfer-keeps-the-modification-time).

## file-time-failure

The copy is only finished when the new file bears the time of the one it was copied from: the
comparison by time takes a file with another time for a changed one. A time that cannot be set
used to be a line in the log and nothing else, so the report and the proof file claimed a
clean run. Now it is an error of the report, with the file and the reason
(FilesystemUtil.md#write_time_refusal). The copy still counts: the content arrived and was
verified, and taking it back would leave the user with the older file. The free
set_last_write_time() hands the reason of its last attempt back for that; a stop request
during the retries is no failure.

## attribute-failure

fs::copy_file gave the new file the permission bits of its source, and the block loop that
replaced it does the same once the copy is verified. A failure used to be swallowed: the
target then had the rights the umask gave it, a script lost its execute bit or a private file
became readable for others, and the run still counted as clean. It is an error of the report
now, with the file and - when the system names one - the reason
(FilesystemUtil.md#permissions_refusal). As with the file time the copy counts, because the
content is there. A later pass does not catch up on the rights: they are not compared.

## restamp

Two local files of the same size and the same content, of which one has another time than
expected, differ in nothing that a copy could mend: the time is set and nothing is copied.
This is how the pass after a file-time-failure catches up, and it does not depend on a state
that a plain one-way item does not have. The side that looks changed gets the time of the
other: the target when it is the newer one (DestModified), the source only when a state says
that it changed and the target did not. A one-way source that looks newer is copied as before.
The contents are compared first, so a target the user edited keeps its time and is handled as
a newer target. A refusal is reported again and ends the work on the file. The preview makes
the same comparison and plans nothing for such a pair.

unstamped_ holds the files whose time could not be set in this pass. For them record_pair()
writes the time the file was meant to get instead of the one it has: with the real time the
state would call the pair in sync for good, with the intended one the next pass sees the side
as changed and comes here.

## reported-once

A target that is newer than its source is left alone unless the item copies
newer targets or syncs both ways. Without a message the user cannot tell why
the file never arrives. Continuous sync meets the same file every cycle, so the
target is remembered and reported only once; it is forgotten as soon as the
file no longer differs this way, so a recurrence is reported again.

The same bookkeeping serves every difference that is left alone on purpose, one
ReportedOnce per kind of message.

## content-conflict

A two-way item whose files have the same time and size but different content
(see FileDifference.md#content-conflict) is not copied in either direction. It is
an error because the two sides stay different until the user decides which one
is right, and it goes to the sync report as well, so a backup proof shows it.

## state

A two-way item passes the SyncState of its last pass down to every SyncFile
(SyncBase.md#state_). Before the comparison the file is marked seen - whatever happens to it,
its entry must survive the pruning at the end of a complete pass (SyncState.md#forget_unseen)
- and its entry is handed to FileDifference as the base (FileDifference.md#three-way). Two
decisions exist only with a base: a source deleted since the last pass deletes the target, a
target deleted since the last pass deletes the source (PlannedAction::DeleteInSource), each
through the same deletion path as delete_in_target, so the recycle bin applies.

After the pass's verdict the state learns what it saw: a file that is Equal is recorded with
the measured sizes and times, a copy records both sides as observe_file() finds them after
the copy, a deletion or a move forgets the entry, a file gone on both sides is forgotten too.
Only a real run records; the preview and the counting pass leave the state as they found it,
so a preview never makes a later run think the sides were in sync.

## both-modified

Both sides edited since the last pass (FileDifference.md#three-way): neither copy is safe.
With conflict copies switched off (SyncItem.md#conflict_copies_) it is reported like the
content conflict, once per file and as an error in the proof, and left alone until the user
settles it; the state keeps the old entry, so the conflict is noticed again on every pass
until then. With them on, see conflict-copies below.

## conflict-copies

A file edited on both sides (BothModified) or differing in content under equal time and size
(ContentConflict) is settled the way every synchronisation tool the user may know settles
it: the version written last wins and is copied over - on a tie the source, so that the
outcome is the same on every pass - and the other version survives under a conflict name
next to it, on both sides, so that neither place lacks anything and the user can merge where
they like. The losing side's file is renamed first, then the winner copied, then the conflict
copy carried to the winning side and recorded in the state like any file. The warning that
names the copy goes into the log and the pass's notes (Synchronize.md#notes), since a conflict
copy nobody notices is a merge nobody does. The copies count as copies of the run; the
preview shows one step with the winner's size.

## conflict-copy-name

"<stem> (<marker>)<extension>", the marker being the translated word for conflict, the date
and the name of the computer whose version lost - this computer for a local side, the server
for an SSH side - so that a user with a laptop and a desktop sees whose edit was set aside.
A name already taken gets a counter, checked on the side the copy is made. Without a message
catalogue - a build without gettext, a test process that bound none - translate() hands the
key back, and a key is no word for a file name, so the English marker takes over then.

## kept-instead-of-deleted

Deleted on one side but edited on the other since the last pass: the plain SourceNotExits or
DestNotExists stands and the edited file is copied back, as it would be without a state. The
warning tells the user why a file they deleted came back.

## parent-of-a-kept-file

A file edited on one side after its directory was deleted on the other is copied back
(kept-instead-of-deleted above) into a directory that no longer exists there - SyncTree does
not recreate a deleted directory (SyncTree.md#prepare-and-settle), only the file's own copy
does, with every missing level. Only this case pays for the check; the ordinary copy relies on
the directory the tree pass created. A directory that cannot be created is reported with its
reason (SyncBase.md#local_dir_created) and the copy is not tried: it could only fail, and
"copy failed" would hide the cause.

## deferred-deletions

With a state, a deletion is not carried out where it is found but noted in the pass's notebook
(SyncState.md#pass-notebook) and carried out by SyncBase::finish_pass once the whole tree was
compared: a file that turns up later under a new name can still claim its old entry (renames),
and the mass-deletion check sees all deletions of the pass at once (SyncBase.md#mass-deletion).
A file item has nothing to wait for and deletes at once. In a counting or preview pass the
deferred deletion becomes a planned step instead.

## renames

A file new on one side whose size and time match an entry of the state whose file has gone
from that side is the same file under a new name: it is renamed on the other side instead of
copied and the old one deleted. A one-way item renames only when it deletes in the target,
since otherwise the old name stays there anyway; a pass-only state never renames
(SyncState.md#pass-only).
