# SshAccount

Module comment for SshAccount.h / SshAccount.cpp.

## host_key_

The server's expected host key as an OpenSSH public key line, empty for the accounts the user
configures: those are checked against known_hosts as before. A phone's account carries the key
the phone sent when it paired (Device.md#Device), because a phone changes its address and
known_hosts files keys under addresses (SshHostKeyVerification.md#verify_pinned_host_key).

## port_

22 unless the configuration says otherwise (Config.md#ssh-port); a phone's SFTP server cannot
bind a port below 1024 and announces its own with every hello.
