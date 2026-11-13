# HideToBackgroundTest

Module comment for HideToBackgroundTest.cpp.

## ImHintergrundBleibtDasFensterOhneTrayErreichbar

"View > Background" may hide the main window only where a tray icon can bring it back
(MainWindow.md#hide-without-tray). On the private X display of the GUI tests there is no tray,
so the window has to stay shown there; with a tray it has to disappear. Whether the window is
really minimized is not checked: that is the window manager's doing, and the test display has
none.

## HIDE_TIMEOUT_MILLISECONDS

How long the test waits for the window to disappear. Without a tray this wait always runs to
its end, so it is kept short.

## window-restored

Restored before the expectation is checked, so that the following tests find a visible main
window even if this one fails.
