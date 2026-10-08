# ViewsTest

Module comment for ViewsTest.cpp.

## AnsichtWechselZeigtGleicheSynchronisationen

Each switch is checked twice: the new view shows both synchronisations, and the previous view
is gone - MainWindow destroys the old window rather than hiding it, and a leftover would keep
answering findSyncItem() for the wrong view.

## editAndDeleteEnabledFor

Same construction as in TreeViewContextMenuTest: the native popup cannot be driven, so the
menu is built like ListWindow/GridWindow::ShowContextMenu build it and primed the way
PopupMenu() primes it, which runs MainWindow::onUpdateSyncItemSelected for the invoking window.

## context-menus

The list and the grid have the same two entries as the tree, guarded by the same update rule,
but each view answers "what is selected" differently (ListCtrl::getSelected, the grid cursor).
The grid test moves the cursor through the grid's API: a click on a cell would start the
in-place editor on some platforms, and the cursor is what GridCtrl::getSelected reads.

## grid-editing

The grid writes straight into the configuration when a cell edit ends, so the configuration is
the thing to read afterwards. The empty source is the one value the grid rejects, with a
message box and the old value put back - both are checked, because a rejected value that stays
on screen would look accepted.

## BaumZeigtDateiUndOrdnerSymbole

A synchronisation of type Datei ends in a file leaf, the other types in a folder; the image
index is what the tree shows. The folder is compared against the file index only: which of the
folder images a node carries depends on whether it is expanded.
