# SettingsTest

Module comment for SettingsTest.cpp.

## EinstellungenKommenInDieKonfiguration

The four start options and the three message filters only reach the configuration; nothing
in the application reads show_errors/show_warnings/show_infos yet, so the round trip into Settings is all that can be tested for them.

## WerkzeugleisteZahnradOeffnetEinstellungen

The gear at the right end of the toolbar shares id::EditSettings with the menu item, so it
must open the very same dialog. Cancelled, so the configuration stays as it was.

## MinimierenStattSchliessenVerstecktAnstattZuBeenden

Close() runs MainWindow::onClose with a vetoable event, exactly like the window's close
button. Without the option the window would leave the main loop, which is why only the
minimize path is tested in-process; the real exit is left to the plan's open points.

## EndgueltigLoeschenLoeschtOhnePapierkorb

Counterpart of SettingsRecycleBinTest: the same delete_file() with the other delete mode
must not add to the recycle bin. The count is compared, not emptiness, so a bin left over by
another test does not matter.

## RestoredRunEntry

Autostart writes the real HKCU Run key - there is no sandbox for the registry - so the
entry's previous state is captured and restored, whether the test passed or failed. The
entry has to point at the running program, which is GuiTests.exe here.
