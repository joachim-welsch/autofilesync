# BackgroundServiceMenuTest

Module comment for BackgroundServiceMenuTest.cpp.

## background_service_menu

The menu bar owns what Menu::create() appended, so the item lookup yields the same wxMenu that
MainWindow::on_menu_open compares the opened menu against.

## open_menu

A menu bar cannot be opened headlessly (see UserActions.cpp's activate_menu_item()), so this sends
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

## start-obstacle

A service whose account can no longer log on - the installer stored the Windows password of the
installing user, and it has changed since - fails every start, in the menu as well as in the
test. That is a flaw of the machine, not of the menu, so the test skips and quotes the message
the user would see; see RestoredBackgroundService.md#start_obstacle for why the probe is a
start.
