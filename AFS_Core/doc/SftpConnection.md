# SftpConnection

Module comment for SftpConnection.h / SftpConnection.cpp.

## SftpConnection

A handle, not the session itself: what it owns is a share in a Session, so that several
handles - the one a copy holds, the one an inspector holds, the one a listing holds - can use
the same open connection within a pass. Without a scope a handle is the only share and the
session goes when the handle goes, which is the behaviour every caller was written for.

## Session

The libssh session with its SFTP channel, released when the last share lets go. Kept out of
the header so that the handle can be used where libssh's types should not spread.

## connect

Takes the pass's session for the account when an SshSessionScope is alive on this thread and
that session is still connected; otherwise connects, verifies the host key and logs in, and
hands the new session to the scope for the next caller. Throws when a step fails - a
RemoteFailure with the reason where one is known, SshUnreachable when the host cannot be
reached at all.

## SshSessionScope

While one is alive on a thread, the sessions opened on that thread are kept in a thread-local
pool keyed by account (name, user and host, since a renamed account may point elsewhere) and
shared; the outermost scope's end closes them all. Thread-local, because the GUI thread
opens sessions of its own for the SSH directory dialog and a session must never be used from
two threads. Nested scopes are harmless: only the outermost one owns the pool. On Windows the
libssh2 modules still open a session per operation; the scope exists there so that
Synchronize reads the same on every platform.

## dropped-session

A session the server closed in the meantime - idle timeout, network gone - fails the operation
in progress the way it always did, and connect() notices the next time: ssh_is_connected()
says no, the stale share is replaced by a fresh login, and the pass goes on. No retry of the
failed operation itself; the pass reports it and the next pass repeats it.

## sftp_connect_count

Test hook: how many real logins happened, so a test can prove that a pass over many files
logs in once (SyncSshTwoWayTests.md#sessions).

## libssh-message-ids

The failures libssh reports with a text of its own keep their own message ids (libssh_*), ending
in ": " for that text; the failures without one share their ids with the libssh2 modules - see
SshFileInspector.md#shared-message-ids.

## SshUnreachable

The host could not be reached: switched off, no network, name not resolvable. Kept apart from
the other failures because it is an expected state of a target that is not always there - the
callers log it as a warning - while a rejected password or host key needs a person to act.

## RemoteFailure

A failure of the other side with its reason (FilesystemUtil.md#FileInfoResult), so that the
caller can do what the reason calls for (CopyLog.md#reach) instead of only knowing that
something threw. It is a std::runtime_error, so every place that caught one still does. A
refused login is AuthenticationFailed; a step without a reason of its own - the host key,
the SFTP subsystem - carries UnknownError and is reported with its text alone.
SshUnreachable is a RemoteFailure whose reason says why the host was not reached.

## connect-refusal

libssh says that the connect failed and gives a sentence. Which of three things happened is
asked of the system instead of read out of that sentence: a name that does not resolve is
NetworkUnavailable - the usual cause is no network at all -, a failure that took as long as
the timeout allows is ConnectionTimeout, and anything quicker is ServerUnreachable, a host
that answered "nobody listens here".
