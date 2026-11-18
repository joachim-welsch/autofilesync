# SyncSshFailureTests

Module comment for SyncSshFailureTests.cpp.

Failures on a server that answers: the unreachable server has its tests
(SyncDirTests, CheckPathTests), these are about a server that is there and refuses one
operation. They need the SSH test server (SSH_TESTS_README.md) and rely on its user being
another one than the user who runs the tests - the refusals are plain file permissions between
the two.

## SyncSshFailureTest

One directory the test user owns, below the temp directory the SSH user can reach. "lokal" is
the local side, "server" the side that is addressed through the account.

## pinned-account

The same server under a second account that carries its host key. A pinned host key is what
makes a copy go through a part file (SyncFileSsh.md#part-files); the key is fetched from the
server at the start of each test.

## listing-refused

Mode 711: the SSH user may enter the directory and look up a name in it, but not list it. That
is the state in which the listing fails while every single file can still be found.

## UnlistableRemoteSourceDirIsReportedAndDeletesNothingItStillHas

With "delete in target" an empty listing looks dangerous. It is not - a target-only file is
deleted only after its own source file was looked up - and the test pins that: the file that
exists on both sides stays. What was wrong is that such a run counted as clean although it
could not see a single source file (SyncFileSsh.md#listing-failure-is-reported).

## sticky-directory

Mode 1777, like /tmp: the SSH user may create files but may neither remove nor replace the
file of another user. The part file can be written; putting it in place of the old file fails.

## PartFileThatCannotReplaceTheTargetLeavesTheOldFile

The last step of a copy through a part file fails. The old file must be untouched, the part
file must be gone, and the run must count as failed.

## RemoteFileAndDirectoryThatCannotBeRemovedNameTheirReason

The server directory belongs to the user running the tests and gives the SSH user no write
permission, so the server refuses both removals. The mirror's deletion is reported with the
remote file and "permission denied" instead of only "deleting failed", and the removal of an
empty directory gives the same reason (SyncFileSsh.md#removal-refusal).

## TimeAndRightsTheServerWouldNotSetAreReportedAndTheCopyStays

The file on the server belongs to the user who runs the tests and is writable for everyone,
so the SSH test user can overwrite it but may set neither its time nor its rights - a real
refusal by the system, no injected one. The new content is there and counts as a copy, and
the pass reports both, each with the path the user configured
(SyncFileSsh.md#unkept-is-reported). Skipped where the SSH test user owns the file, because
then nothing is refused.

## AnUploadWhoseTimeAndRightsWereSetReportsNothing

The counterpart: a file the SSH user creates is its own, time and rights are set, and the
report stays empty - the new errors must not appear on an ordinary upload.

## TheServerTellsTheRoomAFileHasAndCountsTheFileItReplaces

The server is this machine, so its answer can be held against the local one: the room for a
new file is what the drive has available, the room for a file that overwrites one of 32
mebibytes is that much more, and it is not more when the old file stays (generation
principle) or the copy goes through a part file (the pinned account). Eight mebibytes of
tolerance allow for what else writes to the disk between the questions.

## AnUploadTheServerHasNoRoomForIsNotTriedAndTheOldFileStays

The server is said to have five bytes. The upload used to empty the old file, fail and
remove the rest; now it is not started, the old content is still on the server and the
report has the shortage (SyncFile.md#disk-full-on-a-server).

## ALinkTheServerRefusesForLackOfRightsIsNotTakenForAServerWithoutLinks

The server directory belongs to the user who runs the tests, so the SSH test user may not
create anything in it: the link is refused with "permission denied", which used to count as
"this server has no links" and was a warning only. It is PermissionDenied now, and the
probe file left nothing behind (SyncFileSsh.md#links-refused). In a subdirectory open to
everyone the same request creates the link. The other outcome of the probe - a drive that
takes files but no links - needs a FAT drive on the server and is not tested.

## ALinkWhoseServerSideCannotBeAskedIsReported

A directory on the server the SSH test user may not look into: the question about the link
in it fails, and the pair is an error instead of nothing at all (SyncLink.md#unasked).
SyncLink is run by itself, since a directory pass would already stop at the directory.

## read_only_mount

The SSH test server is this machine, so a file system that is mounted read-only here is one
on the server: the first "ro" entry of /proc/self/mounts that holds a file everyone may read
(a snap image, most often). That makes these the tests against a real write protection,
which the local ones cannot have (ReadOnlyFilesystemTests.md#ReadOnly). Skipped where
nothing is mounted read-only.

## AReadOnlyFilesystemOfTheServerIsRecognizedAndNamedForARefusedRemoval

The server says "read-only" for the mount and for a path below it that does not exist, and
not for the test's own directory. Removing a file of the mount is refused by the system
itself - nothing can be lost - and the refusal carries the reason, although the server only
answers "failure"; a link that was to be created there is refused with the same reason.

## AReadOnlyTargetOnTheServerIsOneErrorOfThePass

One pass of the real Synchronize with a target below the mount: one error that names the
target as read-only, where the pass used to fail at creating the directory with no reason.
The item is added to and taken out of the configuration by name, since the fixture's
accounts live there too.

## ACopyRefusedByAReadOnlyServerNamesTheReasonAndAbandonsTheItem

The sync classes run without the check before the pass, which stands for a server that
turns read-only in the middle of one. Three files for a directory of the mount: the first
copy fails, its error names the write protection and the skipped rest, and the other two are
not tried. The wildcard keeps the pass to the test's own names in a directory that is not
the test's. The server is said to have room, so that the copy is really tried: the
mount itself reports none, which the check before an upload would find first.

## ADirectoryRefusedByAReadOnlyServerNamesTheReason

The same for a target directory that would have to be created below the mount.

## AReadOnlyServerWithoutRoomIsCalledReadOnlyAndNotFull

The read-only mount has no room either, which the check before an upload finds first. The
error must be the write protection, with the rest of the item skipped, and not "disk full"
(SyncFile.md#no-room-on-a-read-only-drive).
