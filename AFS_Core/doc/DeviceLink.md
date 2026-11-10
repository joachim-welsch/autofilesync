# DeviceLink

Module comment for DeviceLink.h / DeviceLink.cpp.

## DeviceLink

The PC's end of the control channel to the phones: a TLS server that pairs a phone and takes
its hello. One per process, because one port and one role (see below) belong to one process.
The channel carries only short JSON lines; the files themselves go over SFTP in a later phase,
against the server the phone opens for a pass.

## role

Only one process may listen - the GUI and the background service share the configuration and
would otherwise both answer the phone. The role is `devicelink.lock` in the shared directory (on
Windows a mutex in the user's private namespace, UserObject.md#userobject), taken for as long as
the link runs, after the pattern of SyncArbiter.md#per-user-lock-path.
The process that finds the role taken simply does not listen and says so in the log, once:
the background service retries every minute (MyDaemon.md#device-link) and would otherwise
fill the log while the GUI holds the role.

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

A session is one line in, one or two lines out; the accept loop handles it inline and polls
the listener every half second so that stop() is prompt. A second phone that arrives meanwhile
waits in the backlog for a moment, which is cheaper than a thread per connection. Until the
first line is in, the socket timeouts are three seconds: that is what any peer on the network
can hold the link for with a connection it never finishes, since the fingerprint is checked
only after that line. A known phone then gets the ten second timeouts of a real session.

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
the request, so the user sees the name the phone chose for itself; both are checked
(Device.md#valid_device_id, Device.md#valid_device_name) before the token is spent.

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
one when none is open. The dialog calls it when it opens and again when the token has run out
(PairDeviceDialog.md#renewed-code), so the code on the screen is always one the link accepts.

## take_paired

The device that paired since the last call, for the dialog's timer: the link's thread must not
touch the GUI, so the dialog polls instead of being called back. The device is read from the
shared file (see shared-files), so it reaches the GUI also when the background service paired
it; take_paired() then adds it to this process's configuration, and the GUI's autosave writes
it to the file the service reloads.

## shared-files

The GUI and the background service each run a link, but only one holds the role. The other
one must still be able to pair a phone and show when a phone last reported, so the pieces that
cross the process boundary live as small files in the configuration directory (default_dir(),
the same for both processes since both run as the user): `pairing.token` (token and expiry,
owner-only, DevicePairing.md#files), `pairing.done` (the device a pairing produced, until the
GUI takes it), `devicelink.port` (the port the hosting process listens on, for the QR code) and
`devices.presence` (the last contact of each phone). The hosting link removes the port and
presence files when it stops; a stale port file after a crash is harmless, because a process
that can take the role overwrites it.

## discovery

A phone whose stored addresses no longer reach the PC - the PC got a new address from the
router - broadcasts "AFS-DISCOVER 1 <fingerprint>" over UDP to port 52381. The link answers
with "AFS-HERE 1 <fingerprint> <port>" to the sender, but only when the fingerprint is its own,
so a stranger learns nothing about the PC. The phone takes the address the answer came from.
Plain UDP rather than mDNS: it needs no Avahi or Bonjour library on any platform, and the
firewall rule for the port (TCP and UDP) is the one the pairing already needs. Tests bind an
ephemeral discovery port so they never meet a running program.

## start

Sets the shared directory, loads or creates the identity, takes the role, prepares TLS,
listens and opens the discovery socket; false when any step fails, with the role released
again. The directory is set even when the role is taken, so the pairing still works through
the shared files (hosted_elsewhere()). A discovery port that is taken costs only the
discovery, not the link. Calling it while running is harmless and returns true, so
every place that needs the link may just call it.
