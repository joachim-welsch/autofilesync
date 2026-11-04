# UsbPhoneSyncTest

Module comment for UsbPhoneSyncTest.cpp.

## UsbPhoneSyncTest

The end-to-end use case with a real phone on the USB cable (UsbPhone.md): connect the phone,
choose a template, synchronise. The fixture undoes everything before the base class compares
the application state: the item and the device leave the configuration, the link stops, the
test folders on the phone and the PC are removed, and UsbPhone restores the app's pairing. The
test is skipped when no phone is ready (UsbPhone.md#check), so the suite runs without one.

## phone-folder

A folder of its own at the top of the phone's shared storage, with a random name, so that the
test never touches a folder that holds the user's files and a crashed run does not collide with
the next one.

## pair_over_usb

Instead of the QR scan the test takes the dialog's offer, replaces the addresses by 127.0.0.1
(UsbPhone.md#reverse) and opens its content in the app; the user's part on the phone is the
tap on "Connect". Everything else - token, certificate pinning, the dialog switching to the
templates - runs as for a scanned code. The test waits for the dialog's paired() rather than for
the device in the configuration: the device is there a moment before the dialog's timer takes
it, and a dialog closed in between does not open the templates.

## folder

"Keep a folder the same on both devices": one file only on the phone, one only on the PC; the
pass started with the app's "Synchronise now" must leave both files on both sides. That covers
pairing, the template, the hello with an open SFTP server, the pass in both directions and the
progress back to the phone in one use case.

## only-test-folders

The test runs against a phone that holds the user's files, so a step that could widen the pass
beyond the test folders stops the test before the pass: the template's folders are entered
with ASSERTs, a dialog whose folders did not take is cancelled, and the item is checked for
exactly the two test folders before "Synchronise now" is tapped. An earlier version continued
after the phone folder field kept its preset /Documents and wrote a test file into the user's
Documents folder.

## sftp-port

The app's SFTP server prefers port 52382 and takes any free one only when that is taken; the
test forwards the preferred port, so a phone where something else holds 52382 fails the pass.
