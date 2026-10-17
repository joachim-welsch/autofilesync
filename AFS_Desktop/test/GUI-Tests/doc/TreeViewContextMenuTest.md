# TreeViewContextMenuTest

Module comment for TreeViewContextMenuTest.cpp.

## edit_and_delete_enabled_for

Mirrors Canvas::show_context_menu's own item set and how wxWindow::PopupMenu() primes it
(SetInvokingWindow(), then UpdateUI()) - the actual native popup cannot be driven headlessly
(see UserActions.cpp's activate_menu_item()), so this exercises the same wxUpdateUIEvent path
without opening one.
