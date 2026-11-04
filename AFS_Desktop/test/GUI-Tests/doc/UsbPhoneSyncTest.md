# UsbPhoneSyncTest

Module comment for UsbPhoneSyncTest.cpp.

## UsbPhoneSyncTest

The phone use cases end to end with a real phone on the USB cable (UsbPhone.md): connect the
phone, set up a synchronisation, run passes from the app. SetUp skips the test when no phone is
ready (UsbPhone.md#check), so the suite runs without one. TearDown undoes everything before the
base class compares the application state: every item that names the test phone and the device
leave the configuration, the link stops, the test folders on the phone and the PC are removed,
and UsbPhone restores the app's pairing. Out of reach over the cable: the background schedule
and the trigger on new photos (WorkManager decides when they run), the "Wi-Fi only" and
"charging only" settings, and the discovery of a PC with a new address (UDP broadcast).

## phone-folder

All test folders live below one folder at the top of the phone's shared storage, with a random
name, so that the test never touches a folder that holds the user's files and a crashed run does
not collide with the next one; the PC side lives below the sandbox's target directory.

## pair

The connect dialog with the USB pairing (pair_over_usb) and the template page that follows a
pairing; by default the template page is cancelled, and the test sets its item up through the
phone list (set_up), so both ways to the templates are covered.

## pair_over_usb

Instead of the QR scan the test takes the dialog's offer, replaces the addresses by 127.0.0.1
(UsbPhone.md#reverse) and opens its content in the app; the user's part on the phone is the
tap on "Connect". Everything else - token, certificate pinning, the dialog switching to the
templates - runs as for a scanned code. The test waits for the dialog's paired() rather than for
the device in the configuration: the device is there a moment before the dialog's timer takes
it, and a dialog closed in between does not open the templates.

## set_up

"Set up synchronisation…" in the phone list opens the template page for the paired phone; the
test chooses the template first, because the choice resets both folders, then the folders.

## only-test-folders

The test runs against a phone that holds the user's files, so a step that could widen a pass
beyond the test folders stops the test before the pass: a template or phone folder dialog whose
folders did not take is cancelled, and every item is checked for exactly the two test folders,
in the direction of its template, before "Synchronise now" is tapped (item_of). An earlier
version continued after the phone folder field kept its preset /Documents and wrote a test file
into the user's Documents folder.

## sync

One pass as the user starts it: "Synchronise now" in the app, which opens the phone's SFTP
server and reports to the PC; the test waits for the app's worker to finish (UsbPhone.md#pass)
and pumps the GUI meanwhile, since the pass runs in this process. The SFTP port is forwarded
once per test.

## sftp-port

The app's SFTP server prefers port 52382 and takes any free one only when that is taken; the
test forwards the preferred port, so a phone where something else holds 52382 fails the pass.

## folder

"Keep a folder the same on both devices", set up from the template page that follows the
pairing: one file only on the phone, one only on the PC; one pass leaves both on both sides,
and the app lists the item with "both directions" and reports the pass as done.

## photos

"Back up photos": files and a subfolder go to the PC, a PC file does not go to the phone, and a
photo deleted on the phone stays on the PC after the next pass - the robustness rule of the
concept (ANDROID-CONCEPT.md, aim).

## music

"Music to the phone": the phone ends up with exactly the PC's music - a file that exists only on
the phone is removed, as delete_in_target asks - and a song removed on the PC disappears from the phone with the next pass.
One-way items carry no state (Synchronize.md#sync-state), so the mass-deletion guard does not
apply here; see mass-deletion.

## mass-deletion

A two-way folder whose PC side was emptied: the pass refuses to delete the twelve files on the
phone (SyncBase.md#mass-deletion, at least MASS_DELETION_MIN_FILES and more than half of the
known files) and the app reports the pass with errors.

## two-way

The second pass of a two-way folder carries a changed file from the phone to the PC, a file
renamed on the phone as a rename (track_moves) and a deletion on the PC to the phone. The changed
file gets a different size, so the comparison does not depend on the timestamp resolution.

## conflict

The same file changed on both sides between two passes: both versions survive on both sides,
one of them as the conflict copy (SyncFile.md#conflict-copies). The test compares the contents,
not the names, because the copy's name carries the date and the host.

## own-item

A synchronisation set up without a template: "New synchronisation", the "Phone…" button next
to the source opens PhoneDirDialog, the phone folder is typed there, and the item gets the
phone path; a pass then brings the phone's file to the PC and the app lists the item by name.

## unpair

After a pass, the phone list shows when the phone last reported instead of "not yet"; after
disconnecting there, the app's next "Synchronise now" is refused - the app says the PC no longer
knows the phone, and the new file does not reach the PC.

## closed-pairing

A code whose dialog was closed pairs nothing: closing the dialog ends the token and, without a
paired phone, closes the port (PairDeviceDialog.md#token-ends-with-the-dialog,
#link-lifetime), so the app reports either "not waiting" or "not reachable", depending on which
comes first.

## expired

An offer whose expiry lies in the past: the app says the code has expired and offers no
"Connect" (the check is the app's own, before any connection).
