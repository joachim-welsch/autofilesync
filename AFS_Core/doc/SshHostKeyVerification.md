# SshHostKeyVerification

Module comment for SshHostKeyVerification.h / SshHostKeyVerification.cpp.

## verify_ssh_host_key

Verifies the server's host key against the known_hosts store, rejecting the connection when a
previously stored key no longer matches to guard against man-in-the-middle attacks. Call once
right after the SSH handshake, before authenticating. Only accounts without a pinned key take
this path: those from older configurations, and those saved while their server was unreachable
(SshAccountDialog.md#host-key-confirmation).

## fetch_ssh_host_key

Connects, takes the key the server presents and disconnects without authenticating or touching
any store: the account dialog shows the fingerprint and lets the user decide. The result is an
OpenSSH public key line, the form verify_pinned_host_key() compares against. On Windows libssh2
names the key type by a constant, so the line gets the matching OpenSSH type name.

## host_key_fingerprint

SHA-256 over the decoded key blob, base64 without padding, prefixed "SHA256:" - the form
ssh-keygen -l prints, so the user can compare it with what the server's administrator shows.

## unknown-host-key

A key that known_hosts does not hold yet is the one moment at which somebody in the network can
pose as the server: whoever answers first is believed, and gets the password in the next step.
On Windows such a key is therefore only stored when the registered decision accepts it. The
program registers a question to the user that shows the fingerprint (HostKeyPrompt.md); the
background service registers nothing, so it refuses the connection and says in the log that the
key has to be confirmed in the program once. The tests register an acceptance, which is the
former behaviour.

The decision is called without the lock held: it may wait for the user, and a second connection
that asks meanwhile must not block the thread that shows the question.

The libssh build of the other platforms still stores an unknown key on first use; the decision
is registered there too but not asked yet.

## fingerprint_of

The form OpenSSH prints - "SHA256:" and the unpadded base64 of the hash - so that the user can
compare it with what ssh-keygen -lf or the server's administrator shows.

## known_host_name

known_hosts files a server on a port other than 22 as "[host]:port". The lookup tries that form
first and the bare host name second. The key used to be stored under the bare name whatever the
port, so a second SSH server on another port of the same host was compared with the key of the
first and rejected as changed. Entries stored that way are still found through the second try.

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
