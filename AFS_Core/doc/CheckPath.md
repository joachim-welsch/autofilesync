# CheckPath

Module comment for CheckPath.h / CheckPath.cpp.

## path_exists

must_exist only affects the Windows branch: a source must actually be there, but a local target
that sync itself still has to create must not be rejected for not existing yet - see
side_reachable().

## side_reachable

Reachability of one side regardless of whether it is local or behind SSH; must_exist only
applies to SSH (locally path_exists decides).

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
