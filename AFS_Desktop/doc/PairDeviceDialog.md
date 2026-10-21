# PairDeviceDialog

Module comment for PairDeviceDialog.h / PairDeviceDialog.cpp.

## PairDeviceDialog

The whole pairing from the user's side: a QR code, two numbered steps and a status line. No
address, port or fingerprint to type; the details line shows them in grey only so that a user
with a firewall problem has something to compare against. The dialog polls the link on a
timer because the link's thread must not touch the GUI (DeviceLink.md#take_paired).

## device_identity_dir

The directory of the configuration file, so that the PC's key and certificate live next to
the configuration they belong to (DeviceIdentity.md#load_or_create), in the GUI test sandbox as
in the user's profile.

## start_device_link

The one place the GUI starts the link, used by the dialog and by the main window at start-up
when phones are already paired; the link ignores a second start (DeviceLink.md#start).

## show_offer

A fresh offer each time: when the link cannot be started the dialog says so instead of showing
a code nobody can use (pair_link_unavailable), with the QR code and the steps hidden.

## renewed-code

The token lives ten minutes (DevicePairing.md#begin). A dialog left open longer shows a new
code by itself when the old one expires, rather than a code the phone will report as expired.

## firewall-hint

After a minute without any phone the status adds the hint about a firewall and the port: a
user who scanned the code sees "waiting" forever otherwise, and a blocked port is the usual
cause on Linux, where the installer adds no rule.

## token-ends-with-the-dialog

Closing the dialog ends the pairing window at once, whatever the token's remaining lifetime: a
code photographed from the screen must not pair a phone later, when the user is no longer
looking.

## link-lifetime

The link keeps listening after the dialog when at least one phone is paired, because the
phone's hello needs it; without any phone the port is closed again, so a PC that never paired
a phone has no open port.
