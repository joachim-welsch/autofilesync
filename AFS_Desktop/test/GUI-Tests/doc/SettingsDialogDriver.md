# SettingsDialogDriver

Module comment for SettingsDialogDriver.h / SettingsDialogDriver.cpp.

## SettingsInput

Every setting is optional: a test names what it wants changed, the rest keeps the current
values the dialog opened with. The delete mode goes first because it decides whether the
path field is enabled at all, and typing into a disabled field would silently do nothing.

## checkboxes-by-label

The dialog gives its check boxes no ids, so they are found by the label the user reads, taken
from the same catalogue as the dialog so the driver works in every language. A renamed label
fails the driver with that label in the message, not silently.

## enter_values

Each step reports itself: without that, a failed interaction is indistinguishable from a
broken feature - both end as "the value was not taken over".

## cancel-instead-of-hanging

Leaving the dialog open would hang the test in its modal loop; the cancel result tells the test
that the interaction, not the feature, is what failed.

## fill_settings_dialog

Enters the given settings in the "Globale Einstellungen" dialog the way a user would and
confirms it with OK. Pass the result to ModalDialogWatcher::expect(). The two-argument form
is the original one for the recycle bin.
