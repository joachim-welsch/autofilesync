# Toolbar

Module comment for Toolbar.h / Toolbar.cpp.

## new-first

The first tool is "Neue Synchronisation": it is what a new user needs first, and the
toolbar is where they look for it before they open a menu. Start/Stop stays one slot whose
tool is exchanged (replaceTool), so the id tells the tests and the menu state which of the
two is showing.

## tooltips-without-ellipsis

The toolbar reuses the menu texts for its tooltips. The menu texts end with an ellipsis where a
dialog follows (Menu.md#shortcuts), a tooltip does not, so the ellipsis is cut off here.

## xpm-icons

sudo apt install imagemagick convert stop.bmp stop.xpm edit stop.xpm and rename stop to stop_xpm
