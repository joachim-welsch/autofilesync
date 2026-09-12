# SftpConnection

Module comment for SftpConnection.h / SftpConnection.cpp.

## connect

Connects, verifies the host key and logs in; throws std::runtime_error when a step fails

## libssh-message-ids

The failures libssh reports with a text of its own keep their own message ids (libssh_*), ending
in ": " for that text; the failures without one share their ids with the libssh2 modules - see
SshFileInspector.md#shared-message-ids.
