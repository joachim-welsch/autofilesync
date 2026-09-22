# MainWindow

Module comment for MainWindow.h / MainWindow.cpp.

## onUpdateSyncItemSelected

Shared by EditSync and DeleteSync: both require a sync item to be selected.

## reportBackgroundService

Shared by both background service commands: report success in the status bar, failures in a
dialog because they need the user to react.

## chatDialog_

Kept alive once created: the chat is hidden rather than destroyed on close, so the conversation
survives until the program ends.

## autostart-toolbar-state

onStartContinuousSync() (the manual "Start" handler) always pairs this mode switch with
toolbar_.setStarted(true); this auto-start path used to skip it, leaving the toolbar showing
"Start" while a sync was already running in the background.

## enableAutostart

Writes the .desktop file that starts the program at login.

## disableAutostart

Removes the .desktop file again.

## saveData-single-source

saveData() is the single source of truth for reading the dialog's state (same path onEditSync()
uses) - the old manual reconstruction here had no accessor for compareItems/deleteInTarget and
silently saved content-only comparison and deleteInTarget=false regardless of what the
"Dateivergleich nach:"/"Gelöschte Dateien..." checkboxes actually showed.

## configDialogDir

util::pathOnly() only splits at backslashes, so on Unix it discards the whole path and the
dialog would always start in the home directory.

## reload-after-failed-read

read() resets the configuration before it fails, so the previous one is gone from memory: reload
it, otherwise "Speichern" would overwrite the still intact previous file with an empty
configuration. Skipped for a previous file that never existed, which would only report a second,
pointless read error.

## write-result-checked

write()'s return value used to be discarded here (and at its other two call sites): a failed
save - disk full, permissions - left the user believing "Konfiguration gespeichert" while the
on-disk file was untouched (round 4's atomic write correctly refuses to overwrite it on
failure), and askAndSave() would then let the app close right over the lost changes.

## withJsonExtension

GTK does not append the filter's extension, so a name typed without one would produce a file
that the *.json filter hides on the next open.

## confirmOverwrite

The dialog's own overwrite prompt only saw the name the user typed; an appended extension can
still collide with an existing file.

## copy-written-back

found is a copy; write it back under Config's lock via the original name (the dialog may have
renamed it).

## updateBackgroundServiceMenu-state

The service can also be started or stopped outside of AutoFileSync, so its state is asked for
whenever the menu opens rather than being tracked here. A state that cannot be determined leaves
both commands available: refusing them would lock the user out of a service that may well be
controllable.

## minimize-instead-of-close

Keep the app (and any running background sync) alive in the tray instead of closing - same as
onViewBackground(). CanVeto() is false for a forced close (tray "Beenden", OS logoff), which
must always go through regardless of this setting.

## locateHelpBook

Anchoring the search at the executable rather than at the working directory is what keeps the
help reachable when a desktop launcher, the autostart entry or a debugger starts the program
from an unrelated directory. An installation puts the book next to the executable, and so does
every build (CMake post build step, Windows post build step); walking up to three directories
still finds a book placed at the root of a build tree. The macOS bundle keeps it among its
resources, and the working directory stays a candidate for callers that rely on it.

## help-path-in-message

Names where the book belongs, not the last place searched: that is where a broken installation
gets repaired.

## taskbar-icon-macos

On macOS wxTaskBarIcon addresses the Dock icon, which the application bundle already supplies as
afs.icns in every size the Dock asks for. Setting it here would replace that with the single
64x64 XPM.

## real-shutdown

Always a real shutdown: onClose() already decided to minimize instead and returns before calling
this, so getting here must stop sync and actually exit regardless of the "minimize instead of
close" setting.

## updateSyncState

The second status field belongs to the synchronisation's state (SyncInterface::stateText);
the first keeps the transient messages with their own timer, so a "Konfiguration gespeichert"
never hides what the synchronisation is doing. Refreshed on the timer tick and right after
Start, Stopp and Einmal, so the user does not wait a second for the answer to a click.

## onViewLog

wxExecute reports a command it cannot start through wxLogError, which wxWidgets shows as a
dialog of its own; the user would then see two boxes for one missing editor. Only the
application's box with the message the help describes is kept. Found by
GuiTest.LogDateiAnzeigenOhneEditorZeigtFehler.

## editorExists

On Unix an asynchronous wxExecute forks first and only the child finds out that execvp
fails; the parent gets a process id either way, so a missing editor would never reach the
error box. The program is therefore checked before the start, the way execvp resolves it: a
name with a slash as given, a bare name through PATH. Windows needs none of this - there
CreateProcess fails in the caller. Found by GuiTest.LogDateiAnzeigenOhneEditorZeigtFehler on Linux.

## showLicense

The first thing the status bar says after the start, for five seconds: whom the installation
is licensed to, or "private Nutzung" without a licence file or with one that names nobody
(License.md#private-use). The file is looked for in the
configuration directory first and next to the executable second (License.md#find), and
checked against the product's public key. The welcome message it replaces said nothing the
title bar does not.
