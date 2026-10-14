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

## applyProposal

Dispatches the assistant's proposal (SyncProposal.md#Proposal): a new item goes through
addSyncItemFrom, a deletion through deleteSyncItem with the item looked up by name, so a name
the model got wrong ends in the same "not found" message as a delete without a selection.

## deleteSyncItem

The delete command's confirmation and removal, shared by the toolbar/menu (the selected item)
and the assistant (an item by name): one question, one autosave path, whichever way it starts.

## addSyncItemFrom

The assistant's proposal (see SyncProposal.md) enters the configuration through the same dialog
and the same saveData()/add()/autosave path as a hand-made item, prefilled instead of empty:
nothing the chat suggests is written before the user has seen it in the dialog and pressed OK.

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

write()'s return value used to be discarded here (and at its other call sites): a failed save -
disk full, permissions - left the user believing "Konfiguration gespeichert" while the on-disk
file was untouched (the atomic write correctly refuses to overwrite it on failure), and
askAndSave() would then let the app close right over the lost changes. The checked write now
lives in autosave::save().

## autosave

Every command that changes the configuration ends with autosave::after_change(), so the file is
current the moment the window is - see AutoSave.md. "Datei > Speichern" (menu and toolbar) is
only enabled while Config::changed() is set, which after a change can only mean that the
automatic write failed: the greyed command tells the user that nothing is pending, the enabled
one that a retry is due.

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

## restore-fits-the-screen

The saved geometry may come from a larger screen or from a monitor that is no longer attached;
restored as it was, the window would hang beyond the screen edge or lie on a display that does
not exist. The display is the one holding the saved top-left corner, the primary one when no
display holds it, and the size and position are cut to that display's work area. A saved
position that no display holds is dropped, and the system places the window as on a first
start.

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

## onAbout

The About box names the copyright holder, the website and the licence: it is the one
place a user of the installed program can find out whom the software belongs to and under
which terms they hold it. The details are on the help page "Lizenzbedingungen".
On macOS the standard About panel is used (wxAboutBox), fed from aboutInfo(); it is native
only while the info holds nothing but name, version, copyright and description, so the
website and the licence are part of the description text. The panel is not a wxDialog, so
the GUI test for the message box skips there and checks aboutInfo() instead. Elsewhere the
message box stays: wxAboutBox would also go native on GTK, outside the modal dialog hook
the GUI tests rely on.

## setTitle

The title carries the configuration's path and, while Config::changed() is set, the
"(nicht gespeichert)" marker. With the automatic save that state only remains after a failed
write, and without the marker nothing told the user that the file - the one the background
service reads - still holds the old state. The timer tick refreshes it, so changes made in the grid or in the
dialogs show up without each of them having to call here; SetTitle is skipped while the text is
unchanged so the tick does not repaint the title bar every second. On macOS the window also
shows the state the platform way: the dot in the close button (OSXSetModified) and the file
icon in the title bar (SetRepresentedFilename); both calls are no-ops elsewhere and belong
with SetTitle because they change exactly when the title does.

## askAndSave

Shared by every command that leaves the in-memory configuration behind - opening another file,
closing, starting the background service - with the question as its argument. Since changes are
saved as they happen, the question only comes up after a failed write. Returns false
only when the user cancelled (extra_style wxCANCEL) or the save failed; "Nicht speichern" is a
decision, not a failure. The buttons say what they do ("Speichern", "Nicht speichern",
"Abbrechen") instead of Yes/No, so the question in the caption needs no re-reading; the
return codes stay wxID_YES/NO/CANCEL, which the GUI tests answer with.

## service-reads-the-saved-file

The service reads the configuration file, not the GUI's memory. A sync disabled in the dialog
but not saved was synchronised by a service started right afterwards. So the start first offers
to save, with Abbrechen to not start at all, and a failed save does not start the service over
the stale file either.

## switchLanguage

wxWidgets keeps no message ids, only the texts it was given, so a new language means building
every labelled part again: the menu bar, the toolbar and the view, whose column headers are
texts too. Dialogs are built when they open and need nothing. The enabled state of the
synchronisation menu and the start/stop tool reflect a running sync, not the language, and are
carried over. The view is created anew, which drops its selection.

The chat window is created once and kept, with its conversation, so it stays in the old language
until the next start; recreating it would throw the conversation away. The help book exists only
in German.
