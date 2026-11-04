# UsbPhone

Module comment for UsbPhone.h / UsbPhone.cpp.

## UsbPhone

A real Android phone on the USB cable, driven through adb, for the GUI tests that need the
app at the other end (UsbPhoneSyncTest.md). The cable replaces what a user does by hand: the
QR scan becomes a link the test opens in the app, the taps on the phone's screen become
`input tap`, and the network between phone and PC becomes adb's port forwarding, so the test
needs neither Wi-Fi nor a firewall rule. Everything the object changes on the phone is undone
in the destructor.

## check

The test is skipped, not failed, when no phone is ready: adb missing, no phone or more than one,
USB debugging not confirmed, the app missing, not a debug build, or without "access to all
files", or locked (unlocked). "Access to all files" cannot be granted over adb on OnePlus (ANDROID-CONCEPT.md, measurements),
and without it the app refuses a pass. A debug build is required because only run-as can save
and restore the app's pairing.

## pairing-backup

Pairing with the test overwrites the app's paired PC (PcStore, shared_prefs/pc.xml). The file
is saved before and written back afterwards, or removed when the app had no PC; the app is
stopped first so that it does not write its in-memory copy over the restored file. The device
id, the client certificate and the SFTP host key stay untouched by a pairing, so the phone
afterwards works with the user's PC as before.

## reverse

`adb reverse` makes a port on the phone lead to the same port on the PC. The QR content the
test opens names 127.0.0.1, so the app's TLS connection arrives through the cable at the link,
which in the test scope listens on loopback anyway (DeviceLink.md#loopback-in-tests).

## forward

The other direction: the PC connects for the pass to the address the hello came from, which is
127.0.0.1 here, and the port the hello names; `adb forward` of that port reaches the phone's
SFTP server.

## unlocked

Wakes the screen and dismisses a keyguard without PIN; a phone locked with a PIN, pattern or
fingerprint stays locked, and the test is skipped (check): it cannot tap "Connect" on a locked
screen, and a phone that locks itself during a long suite is no fault of the program. The
keyguard needs a moment to go after the dismiss.

## open_link

The link is the QR content, opened with an explicit package so that no browser takes it and no
app-link verification is needed. The screen is woken again first, since the phone may have
gone dark since check.

## tap

The texts of an app string in every language of the app, read from the app's resources in the
source tree, so the test works whatever language the phone is set to. The screen is read with
uiautomator until a node with one of the texts appears.

## quoting

Every phone command passes two shells: the host's, which popen starts, and the phone's, which
adb starts with the command joined to one line (also for exec-in and exec-out). The phone
command is therefore built with quoted parts first and then quoted as a whole for the host.

## translations_of

AFS_ANDROID_RES_DIR comes from the GUI tests' CMakeLists.txt.
