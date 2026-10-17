# Toolbar

Module comment for Toolbar.h / Toolbar.cpp.

## new-first

The first tool is "Neue Synchronisation": it is what a new user needs first, and the
toolbar is where they look for it before they open a menu. Start/Stop stays one slot whose
tool is exchanged (replace_tool), so the id tells the tests and the menu state which of the
two is showing.

## tooltips-without-ellipsis

The toolbar reuses the menu texts for its tooltips. The menu texts end with an ellipsis where a
dialog follows (Menu.md#shortcuts), a tooltip does not, so the ellipsis is cut off here.

## xpm-icons

sudo apt install imagemagick convert stop.bmp stop.xpm edit stop.xpm and rename stop to stop_xpm

## single-toggle-tool

The start/stop button is one tool (id StartSync) whose bitmap and tooltip set_started() changes in
place; it is never deleted and reinserted. Swapping it for a StopSync tool meant DeleteTool +
InsertTool + Realize, and on macOS that repeated rebuild drove NSToolbar's item list out of range
after enough toolbar operations in one session, throwing NSInternalInconsistencyException from
Realize. Because the tool keeps the StartSync id, its handler (MainWindow::on_start_continuous_sync)
reads the running state and stops when a run is in progress; the separate menu items StartSync and
StopSync keep their own handlers, and the menu disables StartSync while running so only the toolbar
tool acts as the toggle.

## retranslate-in-place

On a language change the tooltips are re-set in place rather than the toolbar being destroyed and
recreated, for the same reason: recreating the native toolbar repeatedly corrupts NSToolbar over a
session. The bitmaps do not depend on the language, so only the short help strings are refreshed,
the start/stop one according to the current running state.
