# SyncItemDialogDriver

Module comment for SyncItemDialogDriver.h / SyncItemDialogDriver.cpp.

## enterValues

Type and wildcard keep the values the dialog starts with ("Datei", "*.*"): they are already
valid, and neither is reachable with plain key strokes. Each field reports itself: without that,
a failed interaction is indistinguishable from a broken feature - both end as "not created".

## cancel-instead-of-hanging

Leaving the dialog open would hang the test in its modal loop; the cancel result tells the test
that the interaction, not the feature, is what failed.

## fillSyncItemDialog

Fills the "Allgemein" tab of a SyncItemDialog the way a user would and confirms it with OK. Pass
the result to ModalDialogWatcher::expect().
