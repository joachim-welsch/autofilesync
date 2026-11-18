# CheckPath

Module comment for CheckPath.h / CheckPath.cpp.

## path_exists

A source (must_exist) has to be there. A local target may be missing, because the sync creates
it, but only the target itself: the directory it is to be created in has to exist, and the
drive has to be mounted. A target with more than one level missing is what a pulled drive looks
like that no fstab lists - "/media/anna/Backup/Fotos" without the stick - and creating the
whole chain would put the backup on the disk the mount point lies on. The same holds for a
mount point that is still there as an empty directory, which the mount table tells apart. A
File item names a file as target; the rule is the same, the file may be missing but not its
directory. Windows keeps its own rule (target-need-not-exist): a drive letter says reliably
whether the drive is there.

## side_reachable

Reachability of one side regardless of whether it is local or behind SSH; must_exist tells the
source, which has to exist, from the target, which the sync may still have to create.

## ssh_path_reachable

Connects to the remote side; must_exist=true additionally requires the path to be present there.
Public so the rule is testable without going through the cache of operator()().

## next_check_interval

Next cache check interval in seconds. Free-standing so the backoff rule can be tested without
actually waiting.

## max-interval-unreachable

Unreachable: the check has to come back soon. With an hour of backoff the sync would stay blind
long after a late mount (boot race) or a network change brought the target back.

## ssh_account_of_path

A path belongs to an SSH connection if the text before the first colon is the name of a
configured account. A drive letter ("C:\...") looks similar but finds no account.

## target-need-not-exist

drive_exists() only ever saw the drive letter, never the path itself: a source folder deleted
underneath an existing drive still counted as reachable, so the pass fell through to SyncDir,
whose exception got reported unconditionally on every single pass instead of once (see
CopyLogTest.UnreachableSourceIsProvenOnceNotOncePerPass). A target still has to be allowed to
not exist yet - SyncDir/SyncTree create it.

## ssh-inspector-connects

The constructor connects, verifies the host key and logs in; it throws if any of that fails.
This is the SSH counterpart of "drive is mounted".

## remote-absence-is-definite

sftp_stat answers for files and directories alike. exists=false means "connected and definitely
absent": for a source that is a reason to pause, not to touch the target, or a deleted source
directory with "delete in target" enabled would empty the target.

## reachability-rules

The source must exist because it is read from. For the target it is enough that the remote side
answers: SyncDir and SyncTree create a missing target directory themselves, and a target yet to
be created must not block the sync.

## operator-ignores-ssh-flag

The path itself tells which side is behind SSH, so ssh is not needed.

## interval-growth

The interval only grows when a re-check confirms the same state; a state change (drive appears
or disappears) resets to frequent checks instead of keeping the old backoff of up to an hour.

## volume-spec-stripping

Strip only a real volume spec ("Label (C:)rest", paren before colon). Merely containing
parentheses ("/home/x/Docs (2023)/f") must not strip, or the path degenerates to a wrong/empty
one and the item never syncs.

## refusal

The reason the last check of a pair of paths failed for, where the server gave one
(SftpConnection.md#RemoteFailure): kept with the cached answer, so that the caller who
reports "not reachable" can say why (Synchronize.md#unreachable-reason). A local path that
is not there has no reason beyond that.
