# WindowAndHelpTest

Module comment for WindowAndHelpTest.cpp.

## ShownAgain

Every test here hides the main window at some point; the next test needs it on screen, so it
is shown again when the test leaves its scope, whether it passed or failed.

## needs-a-tray

The way out to the background and back through the tray menu exists only with a tray. Without
one the window is minimized instead of hidden (MainWindow.md#hide-without-tray), which
HideToBackgroundTest covers, so this test is skipped there, as on the private X display of the
GUI tests.

## activate_task_bar_menu_item

The popup menu of the tray icon is a native menu the simulator cannot open, so the menu is
built the way the icon builds it for a click (CreatePopupMenu) and the entry is found by the
label the user reads; the command it carries is sent to the icon's own handler, which is where
a real pick ends up.

## ClosedHelp

The help book opens in its own frame that would stay on screen for the following tests, and a
second DisplayContents() would only raise it; it is closed when the test leaves its scope.

## about

Two tests: the data the About box is built from (about_info), which holds on every platform,
and the message box itself, which macOS replaces by the native About panel - that panel is
no wxDialog, so the modal dialog hook never sees it and the second test skips there.

## help

Both routes to the help are tested, the menu and the toolbar button: the button used to open
the About box although its icon and tooltip promised the help.

## background-service

Only the "not installed" case, which ends in the "Hintergrunddienst" error box; on a machine with
the service the test skips itself. Start and stop of an installed service are tested in
BackgroundServiceMenuTest.

