# Menu

Module comment for Menu.h / Menu.cpp.

## structure

No submenu for two entries: "Neue Synchronisation…" and "Neue SSH-Verbindung…" stand in the
File menu, as the platform guidelines ask, which saves a click.

The assistant keeps a menu of its own although it has a single entry, against the same
guidelines: tucked into the Help menu it was not found at all - users took it for removed. A
feature nobody finds is worth less than the rule, see the priority of intuitive use.

## shortcuts

The keyboard shortcuts live here, not in the catalogues: they are the same in every language,
and the platform-standard set (Cmd/Ctrl+N, O, S, Shift+S, comma, Q) is what users expect
without reading anything. wxWidgets turns "Ctrl" into Cmd on macOS. Help is Cmd+? there and
F1 elsewhere, as the platforms' own guidelines name it. Menu items that open a dialog or
ask for confirmation carry an ellipsis in the catalogue text; the toolbar strips it again
(see Toolbar.md#tooltips-without-ellipsis).

## background-service-state-entry

Which of the two commands is available already tells whether the service runs, but only once it
is installed at all. The permanently disabled first entry says so outright and also covers the
not installed case.

## phone-entries

"Handy verbinden…" stands in the File menu next to "Neue SSH-Verbindung…": pairing a phone
is, for the user, adding a place to synchronise with, and that is where new places are added.
The paired phones are listed under Edit next to the SSH connections for the same reason.

## restore-entry

"Dateien wiederherstellen…" stands in the Synchronisation menu, set off by a separator: what it
gives back is what a synchronisation deleted or replaced, so that is where someone who misses a
file after a pass looks. It works on files, not on the configuration, which is why it is not
among the settings of the Edit menu.
