# SshAccount

Module comment for SshAccount.h / SshAccount.cpp.

## host_key_

The server's expected host key as an OpenSSH public key line. The account dialog fills it
with the key the user confirmed by fingerprint (SshAccountDialog.md#host-key-confirmation); a
phone's account carries the key the phone sent when it paired (Device.md#Device), because a
phone changes its address and known_hosts files keys under addresses
(SshHostKeyVerification.md#verify_pinned_host_key). Empty only for accounts from older
configurations or saved while the server was unreachable: those are checked against
known_hosts as before.

## port_

22 unless the configuration says otherwise (Config.md#ssh-port); a phone's SFTP server cannot
bind a port below 1024 and announces its own with every hello.
