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
