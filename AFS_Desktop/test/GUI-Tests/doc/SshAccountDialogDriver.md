# SshAccountDialogDriver

Module comment for SshAccountDialogDriver.h / SshAccountDialogDriver.cpp.

## enter_values

Text fields are replaced, not appended to, so the same driver serves the edit dialog, which
comes with the old values filled in. Each field reports itself: without that, a failed interaction is indistinguishable from a broken
feature - both end as "not created".

## cancel-instead-of-hanging

Leaving the dialog open would hang the test in its modal loop; the cancel result tells the test
that the interaction, not the feature, is what failed.

## nested-dialog-already-closed

The click returns only after the nested add dialog has been closed again, so the account is
already in the configuration here.

## fill_ssh_account_dialog

Fills the "SSH-Verbindung hinzufügen" dialog the way a user would and confirms it with OK. Pass
the result to ModalDialogWatcher::expect().

## add_ssh_account_and_close

Presses "Hinzufügen" in the "SSH-Verbindungen" dialog and closes it with OK. The add dialog
opens nested inside this one, so its own expectation has to be announced right after this one.

## rejected-input-keeps-the-dialog-open

OK runs Validate(), and a rejected name, host or user leaves the dialog open behind a
"Validierungsfehler" box. The box is answered by the watcher, the dialog would then wait
forever for the user - so the driver waits for the dialog to close and cancels it if it
does not.

## edit_ssh_account_and_close

Selects the account's row with a click, the way a user does, and presses "Bearbeiten". The
edit dialog opens nested, so its own expectation has to be announced right after this one.

## delete_ssh_account_and_close

Selects the row and presses "Löschen"; the confirmation box is the caller's to answer, so a
test can check its text and try both answers.

## rename_ssh_account_and_close

The in-place rename is the list control's own label editor: EditLabel() opens it on the
row, the new name is typed into it and Return ends the edit, which is what fires
EVT_LIST_END_LABEL_EDIT and the dialog's rename handling.
