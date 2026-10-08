# TreeViewContextMenuTest

Module comment for TreeViewContextMenuTest.cpp.

## editAndDeleteEnabledFor

Mirrors Canvas::ShowContextMenu's own item set and how wxWindow::PopupMenu() primes it
(SetInvokingWindow(), then UpdateUI()) - the actual native popup cannot be driven headlessly
(see UserActions.cpp's activateMenuItem()), so this exercises the same wxUpdateUIEvent path
without opening one.
