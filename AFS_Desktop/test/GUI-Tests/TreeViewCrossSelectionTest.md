# TreeViewCrossSelectionTest

Module comment for TreeViewCrossSelectionTest.cpp.

## LinksclickImBaumZeigtDenPfadAufDerAnderenSeite

A left click on a SyncItem's own node in one tree should reveal and select its counterpart
(source<->target) in the other tree, so both sides always point at the same SyncItem - see
MainWindow::findSyncItem() and Canvas::onItemLeftClicked().
