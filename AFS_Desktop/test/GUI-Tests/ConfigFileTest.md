# ConfigFileTest

Module comment for ConfigFileTest.cpp.

## RestoredConfigFile

Saving writes the sandbox's configuration file, and "Speichern als" and "Öffnen" move the
configuration's path name and the window title elsewhere; GuiTest::TearDown compares the
file and reads it from the sandbox path, so all three are put back when the test leaves
its scope, whether it passed or failed.

## save-as

The native save dialog is answered through ModalDialogWatcher::choosePath with a name
without extension: the ".json" is appended by the application, and because the native
overwrite prompt cannot see the appended name, MainWindow asks itself ("Datei
überschreiben"). Both answers are tested, the file content proves which one won.

## open

Opening first asks about unsaved changes (the message dialog's caption is "Die Daten wurden
nicht gespeichert.", its text "Speichern?"), then shows the native open dialog. A defective
file ends in the message boxes of Config::read - shown with wxMessageBox's default caption
"Message" - and the previous configuration is read back, which the list has to reflect.

## second-file

The second configuration file is written through Config::write instead of "Speichern als":
that route has its own test, and writing directly keeps this test about opening.
