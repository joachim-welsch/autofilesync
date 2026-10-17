# SshFileInspector

Module comment for SshFileInspector.h / SshFileInspector.cpp.

## SshFileInspector

Connects, verifies the host key and logs in; throws if any of that fails, so a constructed
object means the remote side is up.

## inspect_file

remote_path is the path on the remote side without the account prefix ("/home/user/data", not
"account:/home/user/data"); Windows paths are transcribed here ("C:\dir" -> "/C:/dir").
exists=false means "connected and definitely absent"; any other failure throws so that a
connection problem does not look like a deleted file.

## shared-message-ids

The connection failures use the same message ids across all three copies of the SSH code - this
module, SshDirInspector and SyncFileSsh, and SftpConnection for the libssh ones. Each of them
used to word the same failure itself, so a dropped session read differently depending on which
module noticed it. One catalogue entry cannot drift apart the way four did.

Not shared are SftpConnection's libssh_* ids: those texts end in ": " because the caller appends
libssh's own error string, so they are a different message, not a differently worded one.

## path-separators

Configured paths may use either separator (Settings.cpp stores them as entered); the remote sftp
server only understands slashes.

## absence-vs-failure

Only a confirmed "no such file" may report exists=false: the caller (read_ssh_file_info) treats
that as safe-to-delete for delete_in_target. Any other failure (permission denied, dropped
session) must not be mistaken for an absent file - throw into the caller's error path.

## no-double-path-translation

Only the spelling is adjusted ("C:\dir" -> "/C:/dir"). The parameter is the remote path without
account prefix; running get_remote_path() over it again would cut a drive letter path at its
colon ("C:\temp\x" -> "/temp/x"), which only ever worked because the Windows SFTP server looks
up "/temp" on the system drive.

## time_to_iso

Local time to the second, "YYYY-MM-DD HH:MM:SS", for the file information the dialogs show.
Shared with SshDirInspector: it used to format the same mtime through std::chrono, which
prints the nine fractional digits of a system_clock time point and UTC - the SSH directory
dialog showed "07:00:00.000000000" next to the file inspector's local "09:00:00".
