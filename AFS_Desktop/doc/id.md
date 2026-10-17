# id

Module comment for id.h.

## id

IDs for the controls and the menu commands

## stock-ids

Quit, About, Help and EditSettings use the wxWidgets stock ids on purpose: on macOS the
framework moves those menu items into the application menu, where the platform puts
"About", "Settings…" (with Cmd+,) and "Quit", and the GUI tests still find them by id.
They sit before MAINWINDOW because the enumerators after it count on from wxID_HIGHEST + 1
and must not collide with other stock ids.
