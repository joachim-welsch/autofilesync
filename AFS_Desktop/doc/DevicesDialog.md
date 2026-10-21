# DevicesDialog

Module comment for DevicesDialog.h / DevicesDialog.cpp.

## DevicesDialog

The paired phones with their last contact, a button to pair another one and one to unpair.
Changes apply at once and the dialog has only Close - unlike the SSH accounts dialog's OK and
Cancel, which do not undo anything either; a Cancel that cancels nothing misleads.

## status

What the link remembers of the phone's last contact in this run (Device.md#DevicePresence),
"not reported yet" before the first hello. The time is the user's locale format.

## unpair

Asks first, because the phone has to be paired again afterwards, and because the
synchronisation items that will refer to the phone in a later phase stay in place and report
the missing device rather than disappearing.

## rename

The name comes from the phone, but the user may prefer "Annas Handy" to "SM-G991B". Edited in
place in the list, like an SSH account's name, and refused when empty: the name is what every
list and message shows for the device.

## setup

"Synchronisation einrichten…" opens the template page (PhoneTemplatesDialog.md) for the
selected phone, or for the only one: the page is otherwise reachable only right after a
pairing, and a user who chose "later" then needs a way back to it.
