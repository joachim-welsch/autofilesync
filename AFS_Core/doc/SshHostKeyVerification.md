# SshHostKeyVerification

Module comment for SshHostKeyVerification.h / SshHostKeyVerification.cpp.

## verify_ssh_host_key

Verifies the server's host key against the known_hosts store (trust-on-first-use), rejecting the
connection when a previously stored key no longer matches to guard against man-in-the-middle
attacks. Call once right after the SSH handshake, before authenticating.

## known_host_key_type_mask

Maps libssh2's hostkey type to the matching LIBSSH2_KNOWNHOST_KEY_* flag so
libssh2_knownhost_checkp compares against the right key type.
