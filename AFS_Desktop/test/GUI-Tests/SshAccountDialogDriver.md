# SshAccountDialogDriver

Module comment for SshAccountDialogDriver.h / SshAccountDialogDriver.cpp.

## enterValues

Each field reports itself: without that, a failed interaction is indistinguishable from a broken
feature - both end as "not created".

## cancel-instead-of-hanging

Leaving the dialog open would hang the test in its modal loop; the cancel result tells the test
that the interaction, not the feature, is what failed.

## nested-dialog-already-closed

The click returns only after the nested add dialog has been closed again, so the account is
already in the configuration here.

## fillSshAccountDialog

Fills the "SSH-Verbindung hinzufügen" dialog the way a user would and confirms it with OK. Pass
the result to ModalDialogWatcher::expect().

## addSshAccountAndClose

Presses "Hinzufügen" in the "SSH-Verbindungen" dialog and closes it with OK. The add dialog
opens nested inside this one, so its own expectation has to be announced right after this one.
