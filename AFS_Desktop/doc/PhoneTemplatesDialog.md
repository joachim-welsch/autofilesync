# PhoneTemplatesDialog

Module comment for PhoneTemplatesDialog.h / PhoneTemplatesDialog.cpp.

## PhoneTemplatesDialog

"What should happen with this phone?" - the second page of the pairing in the concept, shown
right after a phone connected and again from the phone list. One choice and two folders are
all it asks; the synchronisation it creates is an ordinary item the user can edit like any
other afterwards.

## templates

Three cases cover what people pair a phone for, each with the settings a cautious user would
want and would not find on their own: photos go to the PC and are never deleted there when the
phone loses them (delete_in_target off); a shared folder is two-way with conflict copies and
move tracking; music goes to the phone and is removed there when it leaves the PC's folder, so
the phone does not fill up. All three accept that the phone is away most of the time
(warn_if_unreachable off), because the phone decides when a pass runs (DeviceLink.md#serve).

## phone_template_item

Builds the item without the dialog, so the GUI tests can check the settings of each template
and the dialog stays a thin layer over it.

## unique_name

A second phone of the same model, or the same template chosen twice, must not produce a
duplicate name: the name is the key of a synchronisation.

## defaults-follow-the-choice

Switching the template resets both folders to that template's defaults: a Pictures folder
named after the phone for photos, the Music folder for music, Documents for a shared folder.
A user who typed a folder and then changes the template gets the sensible default for the new
template rather than a photo folder for music.
