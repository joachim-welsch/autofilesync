# SettingsTest

Module comment for SettingsTest.cpp.

## EinstellungenKommenInDieKonfiguration

The four start options and the three message filters only reach the configuration; nothing
in the application reads show_errors/show_warnings/show_infos yet, so the round trip into Settings is all that can be tested for them.

## SpracheSchaltetSofortUmUndKommtInDieKonfiguration

English is chosen because its texts are fixed here whatever language the test session runs in;
"File" and "Global settings" prove that the labels were rebuilt, not just the catalogue switched.
Choosing the operating system's language again through the same dialog restores the language
the other tests expect, and checks the way back at the same time.

## help-follows-language

The help book's title comes from its project file, "AutoFileSync Help" for the English book, so
it shows which book is loaded without opening the help window. Back in the operating system's
language the book of the session is expected again - German in a German session, English in
any other.

## WerkzeugleisteZahnradOeffnetEinstellungen

The gear at the right end of the toolbar shares id::EditSettings with the menu item, so it
must open the very same dialog. Cancelled, so the configuration stays as it was.

## MinimierenStattSchliessenVerstecktAnstattZuBeenden

Close() runs MainWindow::on_close with a vetoable event, exactly like the window's close
button. Without the option the window would leave the main loop, which is why only the
minimize path is tested in-process; the real exit is left to the plan's open points.

## hidden-only-with-tray

Closing hides the window only where a tray icon can bring it back; without a tray it is
minimized and stays shown (MainWindow.md#hide-without-tray). The private X display of the GUI
tests has no tray, a desktop session usually has one, so the expectation follows
wxTaskBarIcon::IsAvailable(). That the window survives is checked in both cases.

## EndgueltigLoeschenLoeschtOhnePapierkorb

Counterpart of SettingsRecycleBinTest: the same delete_file() with the other delete mode
must not add to the recycle bin. The count is compared, not emptiness, so a bin left over by
another test does not matter.

## RestoredRunEntry

Autostart writes the real HKCU Run key - there is no sandbox for the registry - so the
entry's previous state is captured and restored, whether the test passed or failed. The
entry has to point at the running program, which is GuiTests.exe here.
