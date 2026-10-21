# DeviceTests

Module comment for DeviceTests.cpp.

## DeviceLinkTest

Every test gets a directory of its own for the PC's identity, the phones' identities and the
log, and starts the link on port 0 so that two test processes - or a test and the user's
program - never meet on 52381. The link is a singleton; TearDown stops it and removes the one
device the tests register, so the configuration in memory is as before.

## identity

The identity must survive a restart and be private: a regenerated certificate would unpair
every phone, and the key is as sensitive as an SSH key.

## token

The token's whole purpose is to be worth nothing after one use or after a few wrong guesses.

## qr

The QR content is parsed by the app, so its exact shape is pinned here: a change on this side
must be a conscious one on both sides.

## pair

The end-to-end path of a pairing with a real TLS client: the device lands in the configuration
with the fingerprint from the handshake, the dialog gets it from take_paired once, and the
phone saw the certificate the QR code promised.

## hello

A hello is accepted by certificate, not by id: a second phone with the first one's id is a
stranger.

## repair

A reinstalled app pairs again under its old id; the entry is replaced, not duplicated, and the
new certificate is the one that counts from then on.

## config

Devices round-trip through the configuration file with every field, as the SSH accounts do.

## serve

The serve flow end to end against the local sshd, which stands in for the phone's SFTP
server: the device's item names a local directory as "<id>:/path", the hello carries the
test account and serving, and the reply must run items, serve, done, with the files copied
to the PC side and the live account and the run gone afterwards. A hello without serving or
without items ends in idle. The host key the phone sent at pairing is pinned
(SshHostKeyVerification.md#verify_pinned_host_key): with a wrong key nothing is copied.

## local-sshd-key

Read with libssh itself rather than from /etc/ssh, so the test pins the key type the client
actually negotiates with this server.
