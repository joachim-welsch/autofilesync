# SftpConnection

Module comment for SftpConnection.h / SftpConnection.cpp.

## connect

Connects, verifies the host key and logs in; throws std::runtime_error when a step fails -
SshUnreachable when the host cannot be reached at all

## libssh-message-ids

The failures libssh reports with a text of its own keep their own message ids (libssh_*), ending
in ": " for that text; the failures without one share their ids with the libssh2 modules - see
SshFileInspector.md#shared-message-ids.

## SshUnreachable

The host could not be reached: switched off, no network, name not resolvable. Kept apart from
the other failures because it is an expected state of a target that is not always there - the
callers log it as a warning - while a rejected password or host key needs a person to act.
