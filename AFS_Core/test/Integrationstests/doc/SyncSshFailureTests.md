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
