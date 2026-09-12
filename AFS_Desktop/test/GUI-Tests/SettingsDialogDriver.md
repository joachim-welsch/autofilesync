# SettingsDialogDriver

Module comment for SettingsDialogDriver.h / SettingsDialogDriver.cpp.

## enterValues

The mode is set first: it decides whether the path field is enabled at all, and typing into a
disabled field would silently do nothing. Each step reports itself: without that, a failed
interaction is indistinguishable from a broken feature - both end as "the value was not taken
over".

## cancel-instead-of-hanging

Leaving the dialog open would hang the test in its modal loop; the cancel result tells the test
that the interaction, not the feature, is what failed.

## fillSettingsDialog

Enters a recycle bin path and a delete mode in the "Globale Einstellungen" dialog the way a user
would and confirms it with OK. Pass the result to ModalDialogWatcher::expect().
