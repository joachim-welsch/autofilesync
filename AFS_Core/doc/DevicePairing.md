# DevicePairing

Module comment for DevicePairing.h / DevicePairing.cpp.

## DevicePairing

The one-time token that proves a phone has seen the QR code on the PC's screen. One token at a
time: a new begin() replaces the old one, which the pairing dialog does whenever it opens, so a
code photographed earlier is worthless once the dialog has been closed.

## begin

Sixteen random bytes as hex and a ten minute deadline, written to the token file. The deadline
is wall-clock time, because the file is read by another process (the background service may
host the link); a clock change on the PC can therefore stretch or cut the window, which is
harmless for ten minutes.

## files

The token lives in `pairing.token` in the configuration directory, owner-only and replaced
atomically, so the GUI can open a pairing that the background service redeems
(DeviceLink.md#shared-files). The failure count stays in the memory of the redeeming process.

## redeem

Consumes the token on the first match by deleting the file. A wrong token counts as a failure; after FAILURE_LIMIT
of them the token is dropped altogether, so that guessing it over the network - 128 bits, but
still - gets five tries and then needs the user to open the dialog again.

## FAILURE_LIMIT

Five: enough for a phone that resends after a dropped connection, far too few for an attacker.

## PairingOffer

Everything the phone needs to find and trust the PC, assembled by DeviceLink.md#offer: the
addresses and port to connect to, the certificate fingerprint to pin and the token to present.

## qr_content

An https URL to the project's site with the offer in the fragment. A fragment is never sent to
the server, so the token and fingerprint stay between the two devices even when a plain camera
app opens the link; the site shows the app's install links to a phone without the app, and
Android hands the link to the app once it is installed (an app link on the /pair path). The
name is percent-encoded because it is the one free-text field; the rest is hex and digits.

## local_ipv4_addresses

Every IPv4 address of an interface that is running and not loopback, in the order the system
lists them; the phone tries them one after the other, with a few seconds per address, so an
address that leads nowhere costs the user a wait. "Running" rather than "up": a docker or
libvirt bridge without a client is administratively up but has no carrier. IPv6 is left out of the first version: a home
network reaches the PC over IPv4, and link-local IPv6 addresses need a scope the phone cannot
know. Written for Windows against the documented API without a compiler at hand, like the
watcher backends (ChangeWatcher.md#Backend).
