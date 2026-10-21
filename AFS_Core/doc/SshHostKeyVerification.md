# SshHostKeyVerification

Module comment for SshHostKeyVerification.h / SshHostKeyVerification.cpp.

## verify_ssh_host_key

Verifies the server's host key against the known_hosts store (trust-on-first-use), rejecting the
connection when a previously stored key no longer matches to guard against man-in-the-middle
attacks. Call once right after the SSH handshake, before authenticating.

## known_host_key_type_mask

Maps libssh2's hostkey type to the matching LIBSSH2_KNOWNHOST_KEY_* flag so
libssh2_knownhost_checkp compares against the right key type.

## verify_pinned_host_key

For an account whose expected key is known in advance - a phone sends its SFTP host key when
it pairs (DeviceLink.md#pair) - the known_hosts store is the wrong tool: it files keys under
addresses, and a phone's address changes with every lease. The presented key is compared
with the stored OpenSSH line (type and base64 blob; a comment after the blob is ignored), and a
mismatch is reported like a changed key. On Windows libssh2 hands over the raw blob, whose
base64 form is the second token of the stored line, so no type mapping is needed there.
