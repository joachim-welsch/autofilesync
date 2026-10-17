# SshConnectTimeoutTests

Module comment for SshConnectTimeoutTests.cpp.

apply_ssh_connect_timeout() bounds how long a connection attempt may block the sync worker
(SshConnectTimeout.md). The existing unreachable-host tests use a name that does not resolve,
which fails at once and never reaches the timeout; these two make the connect actually wait.
Unix only: the Windows branch has a different scope (SshConnectTimeout.md#windows-timeout-scope).

## SilentServer

A listening socket that is never accepted from: the kernel completes the TCP handshake into the
backlog, so the client's connect succeeds and libssh then waits for an SSH banner that never
comes - the "dropped off the network mid-handshake" case, and deterministic, because no
network is involved.

## session-of-its-own

SshAccount carries no port and SftpConnection::connect() sets none, so the silent server on
its ephemeral port is reached with a libssh session the test builds itself, with
apply_ssh_connect_timeout() applied exactly as connect() does. The blackhole test goes through
SftpConnection, because port 22 is the default there.

## seconds_until_unreachable

SftpConnection::connect() turns a failed ssh_connect() into SshUnreachable; the measurement
brackets the constructor. Whole seconds are enough: the bound is the configured 15 s plus a
generous margin, so a slow machine does not fail the test, while the case without a timeout
blocks for two minutes or more and fails it clearly.

## AServerThatNeverAnswersIsGivenUpWithinTheTimeout

The lower bound of two seconds is what distinguishes "timed out" from "failed immediately";
the upper bound is the actual property.

## ABlackholeAddressIsGivenUpWithinTheTimeout

192.0.2.1 is TEST-NET-1, which no router forwards, so the SYN usually vanishes and the
connect itself has to time out. A network that answers with "unreachable" at once makes the
test meaningless rather than wrong, hence the skip instead of a failure.
