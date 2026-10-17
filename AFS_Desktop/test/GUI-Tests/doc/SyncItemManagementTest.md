# SyncItemManagementTest

Module comment for SyncItemManagementTest.cpp.

## edit-routes

The edit dialog opens from three places - the Bearbeiten menu with a selected row, a double
click on the row, and a click on the row label of the grid - and each route is a test of
its own, because each has its own code path to the dialog (MainWindow::on_edit_sync,
ListCtrl::on_double_click, GridCtrl::on_modify_entry). The proof is the same: the source typed into
the dialog is in the configuration afterwards, and the view shows it.

## delete

Deleting asks first, and the question names the synchronisation; both answers are tested,
because a confirmation that ignores "Nein" is worse than none.

## rejected-names

The dialog's Validate() runs on OK and rejects an empty or an existing name with a "Fehler"
box, leaving the dialog open. The box is answered by the watcher and the driver then cancels
the dialog (SyncItemDialogDriver.md#rejected-input-keeps-the-dialog-open), so what is left to
check is the message and that nothing was created.
