# BackgroundServiceMenuTest

Module comment for BackgroundServiceMenuTest.cpp.

## backgroundServiceMenu

The menu bar owns what Menu::create() appended, so the item lookup yields the same wxMenu that
MainWindow::onMenuOpen compares the opened menu against.

## openMenu

A menu bar cannot be opened headlessly (see UserActions.cpp's activateMenuItem()), so this sends
the event that opening it produces. MainWindow refreshes the two commands from there, because
the service can be started and stopped outside of AutoFileSync as well.

## no-message-box-on-failed-query

Asking for the state runs launchctl resp. systemctl. Where that command is missing, the failure
belongs in the returned state - not in a message box that wxLogGui would flush during a later
test and fail that one.

## HintergrunddienstmenueZeigtObDerDienstLaeuft

Start and Stop describe opposite states of one service, so which of them the menu offers is the
only clue it gives about whether the service is running. On macOS both were offered: the state
was queried with systemctl, which does not exist there, and an undeterminable state deliberately
offers both.

## state-checked-after-symptom

Checked after the symptom so that a failing run names both: what the user sees in the menu, and
the undeterminable state that leads to it.

## state-line

The state line carries the answer where neither command does: a service that is not installed
offers neither of them.

## HintergrunddienstLaesstSichOhneAdministratorrechteStoppenUndStarten

The installer lets the service user start and stop the service without a password: polkit rule
on Linux, service permissions on Windows. Both commands run in the order that ends where the
service began, and RestoredBackgroundService puts it back should a step fail. A missing right
shows as the "Hintergrunddienst" error box, which the fixture reports as an unexpected dialog.
