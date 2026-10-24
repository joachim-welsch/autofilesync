# DeviceLink

Module comment for DeviceLink.h / DeviceLink.cpp.

## DeviceLink

The PC's end of the control channel to the phones: a TLS server that pairs a phone and takes
its hello. One per process, because one port and one role (see below) belong to one process.
The channel carries only short JSON lines; the files themselves go over SFTP in a later phase,
against the server the phone opens for a pass.

## role

Only one process may listen - the GUI and the background service share the configuration and
would otherwise both answer the phone. The role is a lock file per user (a named mutex on
Windows), taken for as long as the link runs, after the pattern of SyncArbiter.md#SyncArbiter.
The process that finds the role taken simply does not listen and says so in the log; the
first version leaves the role to the GUI, the daemon learns the channel in phase 3.

## use_private_scope

The test build takes the role under a name of its own and in the process's temp directory, for
the same reason SyncArbiter.md#TEST_SCOPE gives: a test must never wait for, or lock out, the
user's running program. GUI tests link the shipped library and switch at run time.

## loopback-in-tests

In the private scope the link listens on 127.0.0.1 only. Listening on every interface makes the
Windows firewall ask the user whether to allow the test program; the question takes the focus,
and every GUI test after it clicked and typed into the wrong window. The fake phone of the tests
connects through loopback anyway.

## port

52381 by default, chosen at random from the unassigned range; when it is taken the link falls
back to any free port, and the QR code and the daemon's later mDNS announcement carry the port
that was really bound. Tests pass 0 and get a free port at once.

## tls

TLS 1.3 only, with the PC's own certificate (DeviceIdentity.md). The phone pins the fingerprint
from the QR code, so no certificate authority is involved on either side.

## client-certificates

The server asks every client for a certificate and accepts any one at the TLS layer; the
callback returning 1 is what makes OpenSSL hand over a self-signed certificate instead of
failing the handshake. The decision is taken afterwards by fingerprint: a pair request records
it, a hello must present the recorded one (Device.md#cert_fingerprint). A client without a
certificate gets certificate_missing and nothing else.

## one-session-at-a-time

A session is one line in, one or two lines out, with ten second socket timeouts; the accept
loop handles it inline and polls the listener every half second so that stop() is prompt. A
second phone that arrives meanwhile waits in the backlog for a moment, which is cheaper than a
thread per connection and cannot be abused to hold the link busy for long.

## protocol

One JSON object per line, UTF-8. Requests: `pair` with token, device (id, name, sshHostKey,
sftpPort) and appVersion; `hello` with deviceId, sftpPort, sftpUser, sftpPassword, serving and
appVersion. Replies: `paired` and `items` carry a pc object (name, fingerprint, protocol
version); `items` lists the device's items (name, direction, enabled); a hello ends with `idle`,
or with `serve`, any number of `progress` (done, total) and finally `done` (completed, errors)
when the phone offered its server and has items (see serve). `error` names a reason:
bad_request, certificate_missing, pairing_closed, token_invalid, unknown_device. Numbers may
arrive as strings, as boost's writer sends them and the app's parser accepts them.

## json-output

Replies are assembled by hand: boost's writer turns an empty array into "" and quotes every
number, and the few reply shapes are simpler to write than to coerce.

## pair

Pairing needs the dialog open (DevicePairing.md#DevicePairing) and a valid token; the device
takes the fingerprint of the certificate it just presented. The phone's id and name come from
the request, so the user sees the name the phone chose for itself.

## repaired-device

A phone that pairs again under the same id - reinstalled app, new certificate - replaces its
entry instead of adding a second one, so the items that will refer to the id keep working.

## hello

The phone announces itself: the certificate must be the one recorded for the id, otherwise the
link answers unknown_device and forgets the request. A known phone updates its presence
(Device.md#DevicePresence) and receives its items and `idle`.

## items

The device's synchronisation items, for the app's status card: every item whose source or
target names the device as its SSH account (Config.md#device-accounts), with the direction
as the phone sees it - toPc, toPhone, or both for a two-way item.

## serve

A hello with `serving` true says: my SFTP server is open on this port with this one-time
password, synchronise now. The link turns that into a live account for the device, marks the
device serving (Device.md#serving) and sends `serve`; the pass then runs in the process's one
synchronisation: a continuous run that is active picks the device's items up on its next
tick (Synchronize.md#device-items, woken at once), otherwise the link creates a single run
restricted to the device (Synchronize.md#device-filter) and destroys it afterwards. The session
stays open meanwhile - the link's accept loop handles one phone at a time anyway - and the
phone receives a `progress` line per finished item, read off the pending set that
Synchronize::sync_item clears through item_passed(). `done` follows when every item ran, when
the run ended early (a stop), or after PASS_LIMIT; the one-time password is forgotten with it.

## offer

Everything for the QR code (DevicePairing.md#PairingOffer), with the current token or a fresh
one when none is open. The dialog calls it when it opens and shows the result; a dialog that
stays open longer than the token lives shows a code the phone will find expired, which the
app reports and the user fixes by opening the dialog again.

## take_paired

The device that paired since the last call, for the dialog's timer: the link's thread must not
touch the GUI, so the dialog polls instead of being called back.

## start

Loads or creates the identity, takes the role, prepares TLS and listens; false when any step
fails, with the role released again. Calling it while running is harmless and returns true, so
every place that needs the link may just call it.
