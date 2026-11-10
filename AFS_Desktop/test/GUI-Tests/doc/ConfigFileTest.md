# ConfigFileTest

Module comment for ConfigFileTest.cpp.

## RestoredConfigFile

"Speichern als" and "Öffnen" move the configuration's path name and the window title
elsewhere; GuiTest::TearDown reads the file from the sandbox path, so both are put back when the
test leaves its scope, whether it passed or failed. The file content itself is restored by
TearDown for every test, since the automatic save changes it in nearly all of them.

## BlockedConfigFile

The only way to an "unsaved" configuration is a write that fails, so the configuration's path
is pointed at a directory for the duration: Config::write writes its temporary file beside it
and then cannot rename over the directory, on Windows and POSIX alike. The temporary file it
leaves behind is removed with the directory.

## add_unsaved_sync_item

Adds an item through the dialog while the file is blocked - the fill action and the answer to
the "Fehler" box are both queued before the menu command, since the box opens from inside the
command - and reports whether the item ended up in memory with the change still pending. The
tests of the save command, the title marker, "Öffnen" and the service start build on it.

## autosave

A change written at once leaves nothing pending: the file holds it, the title carries no
marker, and "Speichern" is greyed - it is enabled only while a change is pending, which the
menu bar reflects after UpdateWindowUI.

## save-as

The native save dialog is answered through ModalDialogWatcher::choose_path with a name
without extension: the ".json" is appended by the application, and because the native
overwrite prompt cannot see the appended name, MainWindow asks itself ("Datei
überschreiben"). Both answers are tested, the file content proves which one won.

## open

Opening first asks about a pending change (the message dialog's caption is "Die Daten wurden
nicht gespeichert.", its text "Speichern?"), then shows the native open dialog. A defective
file ends in the message boxes of Config::read - shown with wxMessageBox's default caption
"Message" - and the previous configuration is read back, which the list has to reflect.

## second-file

The second configuration file is written through Config::write instead of "Speichern als":
that route has its own test, and writing directly keeps this test about opening.

## title

The marker is the translated "title_unsaved" text, looked up at the call like the dialog
captions above. It appears with the failed write - together with the "Fehler" box and the
enabled "Speichern" - and vanishes once the command has written the file.

## background-service

The service reads the file, so a start over a pending change first asks "Jetzt speichern?"
with Ja/Nein/Abbrechen. Abbrechen must stop the start before the service manager is touched -
on a machine with the service installed that would be a UAC prompt no test can answer. The
answered messages count the "Fehler" box of add_unsaved_sync_item as well. Ja is tested where the
service is not installed - the start ends in the "Hintergrunddienst" error box - and where this
user may control the installed service without a password prompt, where the service runs
afterwards. Either way the file content proves that the save came first. An installed service
that cannot start on this machine - RestoredBackgroundService.md#start_obstacle - would end the
start in the same error box; the test skips with its message rather than report the machine's
flaw as a GUI failure.
