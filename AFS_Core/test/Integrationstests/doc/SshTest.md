# SshTest

Module comment for SshTest.h / SshTest.cpp.

## ssh_available

SshFileInspector's constructor connects, verifies the host key and authenticates, throwing on
any failure - so building one is the cheapest honest answer to "is the test server there?".
Cached: the SSH tests would otherwise pay a full connection setup per test just to decide to
skip.

## ssh_available-contract

True when a real SSH server is reachable at SSH_HOST and accepts SSH_USER/SSH_PASSWORD. Probed
once per process, so enabling the SSH sync tests costs a single connection attempt on machines
that have no server.

## SKIP_WITHOUT_SSH_SERVER

The SSH sync tests need a real server (see SSH_TESTS_README.md). They are enabled rather than
DISABLED_, so they run wherever the test server exists and skip - instead of failing - wherever
it does not.

## remove_remote_tree

What an SSH test writes on the remote side belongs to the SSH test user, so the
user running the tests cannot delete it, and the core has no remote rmdir. This
removes a file or a whole directory tree through SFTP as the test user. It talks
to the server with the fixed test account instead of one from Config, because it
runs after the test, when Config may already be reset. Errors are ignored: it is
clean-up, and the leftover check of TestMain.cpp reports what stayed behind.

## remove_remote_tree-windows

On Windows the SSH test user writes into a directory of the local disk whose ACL the new
files inherit, so the user running the tests can remove them with std::filesystem and the
fixture's own clean-up suffices. The SFTP walk above uses libssh, which the Windows build does
not link (it uses libssh2).
