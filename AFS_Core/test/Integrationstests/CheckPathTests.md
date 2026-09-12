# CheckPathTests

Module comment for CheckPathTests.cpp.

## remove_all_retry

Windows releases a directory only once the last handle to it is closed. Retrying briefly beats a
single blind attempt.

## RepeatedHitsWithinValidityWindowDoNotExtendIt

Fehler 10: the cache's backoff interval used to double on every lookup that hit an entry, even
one still within its validity window - not just when the entry actually expired and got
re-verified. That let the interval balloon far past its intended base within a fraction of a
second under repeated polling (e.g. Synchronize's continuous-sync loop), making the cache appear
valid long after it should have expired and been re-checked.

## stale-cache-detection

Remove the source now: further calls only see this if they actually re-check the filesystem
instead of returning the cached (stale) result.

## interval-must-not-grow

Several hits well within the base 2s interval - none of these may grow the interval past its
original 2s, or the cache would stay "valid" (and thus keep returning the stale true) past when
it actually should expire.

## absolute-deadline

An absolute deadline instead of a fixed remaining delta: insensitive to accumulated sleep
inaccuracies.

## windows-checks-path-itself

CheckPath::path_exists() now checks the source path itself on Windows too, not just the drive
letter (see CheckPath.cpp), so this matches the Unix expectation on both platforms.

## UnreachableBackoffIsCappedFarBelowTheReachableOne

An unreachable path used to be treated with the same backoff as a reachable one: the interval
doubled up to 4096s. After a boot race (the daemon starting before the CIFS mount) or a network
change, the sync then stayed blind for over an hour although the target had long been back.

## ChangedResultResetsBackoffToTheBaseInterval

When the state changes (the target appears or disappears), checking has to become frequent again
instead of holding on to the old backoff.

## WithoutVolumeSpecKeepsPathsThatMerelyContainParentheses

Fehler 2 (round 4): without_volume_spec() stripped whenever the path merely contained "(" and
")" - not only for a real volume spec ("Label (C:)rest"). A plain path with parentheses ("Docs
(2023)") degenerated to a wrong or empty path, so the item never synced; worse, the mangled
source looked like SourceNotExits, which deletes target files when delete_in_target is enabled.

## CheckPathSshTest

Registers an unreachable account and - where there is one - the local test account. RFC 2606
reserves ".invalid", so it can never resolve: the connection attempt fails fast and identically
everywhere.

## UnreachableHostIsNotReachable

The heart of the matter: as long as the remote side does not answer, the path is not reachable.
CheckPath used to return true for every SSH path without checking, so an unreachable server
looked like one standing ready.

## UnknownAccountIsNotReachable

A prefix without a matching account is not an SSH path one could reach - it cannot even be
resolved.

## OperatorReportsUnreachableSshSource

An unreachable SSH path has to make the synchronisation pause through operator()() as well -
that is where the result is actually evaluated.

## create-dir-instead-of-assuming

Create the directory instead of relying on one being there: the test is meant to be able to fail
on reachability, not on an assumption about the machine. Spelled the way the other SSH tests
spell it, without a trailing separator.

## trailing-separator

Paths entered with a trailing separator are common and have to give the same result - otherwise
the synchronisation would pause silently although the remote side is standing ready.

## MissingRemoteSourcePathIsNotReachable

Connected, but the path is gone: for a source that is a reason to pause. Otherwise a deleted
source directory would empty the target along with "Gelöschte Dateien im Ziel ebenfalls
löschen".

## MissingRemoteTargetPathIsStillReachable

For a target the connection is enough: a target directory that does not exist yet is created by
SyncDir and SyncTree themselves.
