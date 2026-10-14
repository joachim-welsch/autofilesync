# Menu

Module comment for Menu.h / Menu.cpp.

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
