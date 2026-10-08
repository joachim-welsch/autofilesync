# TestMain

Module comment for TestMain.cpp.

## private-temp-dir

An integration test has to leave the file system as it found it, and that has to
be checked, not hoped for. The check needs a directory nobody else writes to,
and the fixtures take their paths from the temp directory while the process
starts, in static initialisers - long before main(). The test program therefore
starts itself a second time: the first process creates a temp directory of its
own, points TMPDIR to it and runs the tests in a child; when the child has ended
it removes the directory and passes the exit code on. The environment variable
tells the child that it is the one that runs the tests. A debugger that should
see the tests directly sets the variable, and TMPDIR, itself. Windows runs the
tests in the shared temp directory without the leftover check.

## private-home

HOME points into the private temp directory as well, so that nothing a test
derives from the home directory - configuration, key file, default log, recycle
bin - touches the files of the user. known_hosts is copied, as the SSH tests
would otherwise meet an unknown server. The XDG variables are unset, because
they take precedence over HOME.

## ssh-user-writes-here

The SSH tests reach the temp directory through the local SSH server as the SSH
test user, who has to create directories in it.

## dir-the-ssh-user-can-reach

The private temp directory is opened up for the SSH test user, but that only
helps when the user can traverse the directory it lies in. macOS gives every
user a temp directory of their own, readable by nobody else, so the private
directory goes to /tmp there; on Linux the temp directory is the shared one
already.

## LeftoverListener

Compares the names in the private temp directory before and after each test.
Names only: the scratch log grows with every test.

## leaks-fail-in-the-environment

When OnTestEnd() runs, the result of the test is already final, so a failure
raised there would not count. The differences are collected and reported in
TearDown() of the environment, which does make the run fail, each with the name
of the test that caused it.

## ScratchLogEnvironment

Logging goes to a file in the temp directory from the first message on and the
file is deleted at the end. Tests that need a log of their own use ScopedLog,
which returns to this file.

## ScratchDirCleaner

Most fixtures work in the shared directories of TestPaths.h and FileUtil.h and
used to remove only their files, so each test inherited the directories of its
predecessors - and some silently relied on that. The shared directories are
removed centrally after each test; everything else a test creates it has to
remove itself, which the LeftoverListener checks.

## listener-order

googletest calls the listeners in reverse order at the end of a test: the
cleaner is appended last, so it runs before the LeftoverListener compares.

## entries-of-the-ssh-user

What an SSH test wrote on the remote side lies in the same temp directory but
belongs to the SSH test user, so only that user can remove it. Entries with a
foreign owner are removed through SFTP (SshTest.md#remove_remote_tree).

## arbiter-locks

SyncArbiter never unlinks its lock files, since removing a lock file another
process may be about to lock would let both hold it. The files of the test scope
are removed here.
