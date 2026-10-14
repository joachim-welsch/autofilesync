# SyncItemDialogDriver

Module comment for SyncItemDialogDriver.h / SyncItemDialogDriver.cpp.

## SyncItemOptions

Every option is optional: a test names only what it wants changed, everything else keeps the
dialog's own default - the same as a user who opens the dialog and touches one box. The order
in enter_options matters: two-way sync locks "Kopieren wenn Ziel neuer" and the type locks
"Gelöschte Dateien im Ziel löschen", so those come after what decides about them.

## enter_values

Every field reports itself: without that, a failed interaction is indistinguishable from a
broken feature - both end as "not created". Text fields are replaced, not appended to, so the
same driver serves the edit dialog, which comes with the old values filled in. Type and
wildcard keep the values the dialog starts with ("Datei", "*.*") unless the test says
otherwise: they are already valid. The dialog is left on the first page, where it opened.

## enter_options

A locked option fails the driver instead of being set behind the dialog's back: a test that
wants the lock itself reads the box's state in its own action instead of using the driver.

## cancel-instead-of-hanging

Leaving the dialog open would hang the test in its modal loop; the cancel result tells the test
that the interaction, not the feature, is what failed.

## rejected-input-keeps-the-dialog-open

OK runs Validate(), and rejected input (empty or duplicate name) leaves the dialog open behind
a message box. The message box is answered by the watcher, the dialog would then wait forever
for the user - so the driver waits for the dialog to close and cancels it if it does not.

## fill_sync_item_dialog

Fills a SyncItemDialog the way a user would - all three tabs, as far as the input asks for
them - and confirms it with OK. Pass the result to ModalDialogWatcher::expect().
