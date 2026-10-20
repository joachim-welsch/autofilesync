# SyncSshTwoWayTests

Module comment for SyncSshTwoWayTests.h / SyncSshTwoWayTests.cpp.

Two-way sync where one side is an SSH path. The local-only cases live in TwoWaySyncTests; what is
new here is the direction target -> source, because it is the one direction in which SyncFile
passes the remote path as the copy *source* while the SyncItem still calls it the target. The
tests cover an SSH target (the usual "back up to the server" item) as well as an SSH source, and
each of the three item types that behaves differently: File, Dir and Tree.

Like the other SSH suites these tests need the test server from SSH_TESTS_README.md and skip
without it.

## remote-dir-per-test

Each test gets its own remote directory, named after the test: the remote side is the one place
these tests cannot simply delete and recreate (erase_ssh_test_dir removes files, there is no
remote rmdir), so tests must not share a directory whose leftovers would become another test's
"target-only file".

## create_remote_dirs

ssh_mkdir() is a single sftp_mkdir and creates exactly one level, so the base directory has to
exist before SyncBase can create the per-test directory below it.

## seed_remote

The remote side is filled by a normal one-way sync instead of by writing to the remote path
directly: that keeps the tests honest on a genuinely remote server and it is the state a user
reaches, too - first a one-way sync, later two-way.

## remote-time-is-the-transfer-time

Backdating the local file by an hour is the reliable way to make the remote side the newer one:
the seeding sync gives the remote copy the time of the local file it came from, so both sides
start out with the same time.

## modification-time-survives-the-transfer

The property everything time-based over SSH rests on, asserted in both directions. Without it a
freshly uploaded copy carries the time of the transfer and therefore looks newer than the
original it came from - a two-way item would copy it straight back, and the next pass would copy
it up again.

## whole-seconds

SFTP reports the modification time in whole seconds, so the comparison has to happen in seconds
rather than in file_time_type, which carries sub-second precision on the local side.

## compare_by_time

The comparison an SSH item realistically runs on - and the configuration that used to send a
file back and forth on every pass, which is why a repeated pass is asserted for it as well.

## RemoteChangeIsNoticedWithTheDefaultComparison

A default item compares the content only, and the content of a remote file cannot be compared -
so nothing at all was compared and every file that existed on both sides came out Equal. The
criterion degrades to size and time for SSH paths instead (FileDifference.md#ssh-content-fallback);
this test is what pins that a change made on the remote side arrives.

## ssh_source_item

The mirrored item: the remote directory is the source, the local one the target. A file that
exists only on the local target has to travel to the remote source - the same code path as
above, but with the SSH side as the copy destination.

## idempotence

See TwoWaySyncTests.md#idempotence. Asserted for both comparisons an SSH item can run, because
each settles for its own reason: the size is unchanged by a transfer, and the time is carried
along by it.

## state

The local tests' deletion cases (TwoWaySyncTests.md#state) with the target on the server: the
state records the server's write time in whole seconds, as the comparison reads it, so an
unchanged remote file must still count as unchanged on the next pass. The third test rewrites
the file on the server through a one-way upload after deleting it locally; the edit must come
back instead of the deletion going out.

## directories

The directory cases of TwoWaySyncTests.md#target-only-directories and #deleted-directories with
the target on the server: a remote-only directory comes back whole, a directory deleted locally
is emptied and removed on the server (ssh_rmdir, SyncFileSsh.md#ssh_rmdir), and a directory
removed on the server leaves the local side without being recreated remotely.

## renames

A local rename into a new subdirectory becomes an SFTP rename on the server (ssh_rename,
SyncFileSsh.md#ssh_rename), directory created on the way; the server's write time of the file
stays what the upload gave it, which an upload anew could not guarantee to the second.

## conflict-copies

A conflict whose losing version lies on the server: the copy is made there by an SFTP rename,
carries the server's host name (SyncFile.md#conflict-copy-name) and comes back to the local
side like any file. The server's version is planted through a one-way upload so that its write
time stays older than the local edit.
