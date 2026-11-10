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

## sealed_password_

The stored ciphertext of a password that could not be decrypted when the configuration was read
(PasswordCrypto.md#unusable-key). The account has no password then, and the next save would
replace the ciphertext with an encrypted empty string - losing a password whose key may only
have been out of reach for a moment. Config writes this value back instead, until a new
password is set.
