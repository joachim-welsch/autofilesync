# Scenes

Module comment for Scenes.h / Scenes.cpp.

## run_scene

Scenes are looked up by name and run in the order given, each with a fresh dialog watcher:
a scene that did not consume its expected dialog must not hand it to the next. Dialogs the
watcher had to cancel are reported, since they would be visible in the recording.

## scenes

Each scene is one block of the video script: create the synchronisation, preview, start and
the new photo, the window closed while the sync runs (plus the "disk unplugged" and
"photo deleted" moments), the options tour, the SSH dialog, the assistant, the closing
frame. The script's wording is the specification; the marks name the moments it narrates.

## click_tool

The toolbar is where the narration points, so the pointer glides to the tool and rests
until its tooltip shows before it clicks. If the tool's rectangle cannot be found
(Pacing.md#tool_rect) the command is fired directly - the recording then lacks the hover,
but the scene goes on.

## menu-by-keyboard

Menus are opened the way a viewer could follow: F10 opens the first menu, Right arrow moves
along the bar, Down and Return pick the entry. The menu indices follow Menu.cpp - Datei,
Bearbeiten, Synchronisation, Hintergrunddienst, Ansicht, Assistent, Hilfe.

## menu-opened-before-next-key

F10 and the arrow keys open their menu a moment after the key event; a Down sent before the
menu has taken the keyboard goes to the list behind it and is lost, and Return then picks the
wrong entry. Each key that opens a menu is therefore followed by a wait for the window's
wxEVT_MENU_OPEN, which wxGTK sends for keyboard-opened menus as well.

## downs-to-entry

A menu opened from the keyboard comes up with nothing selected, and the arrow keys skip
separators and disabled entries - after Start the entry Start is disabled and Stopp enabled, so
the same entry needs a different number of key presses at different moments. The count is
therefore read from the wxMenuBar right before the keys are sent: one Down per enabled entry
up to and including the wanted one.

## menu-fallback

Whether the keyboard reached the entry is read from the dialog watcher: a command that opens a
dialog has entered one within a few seconds. Otherwise the menu is closed again and the
command is sent as an event, so a keymap or focus problem costs the visible menu but not the
scene.

## pick_directory

The directory chooser is left open long enough to be seen, navigated to the directory with the
native SetPath (GTK shows the folder), then confirmed. The base class SetPath is set as well
before EndModal because the GTK chooser reports its path asynchronously
(ModalDialogWatcher.md#gtk-chooser-is-asynchronous).

## type-choice

The type is selected programmatically after the pointer has arrived on the choice: a real
click opens the native popup, whose entries cannot be addressed by window id, and a keyboard
walk through it depends on the current selection.

## cycle

With cycle_seconds set, the options tab is opened and the cycle entered visibly, otherwise
the scene waits a full default cycle for the new photo and leaves a mark for the cut.

## close-to-tray

Alt+F4 lets the window manager close the window, which the application turns into "hide to
tray" because MinimizeInsteadOfClose is on. Without a window manager (plain Xvfb) nothing
answers Alt+F4, so after a short wait the window is closed programmatically with the same
effect. Show() brings it back for the rest of the scene.

## assistant

The chat is a non-modal dialog found by its input field. The answer is awaited only when a
model is installed (ModelLocation.md#find); the first answer loads the model and can take a
minute on a CPU, which the editor cuts. "Answered" means the whole answer: the first words
arrive while the model is still generating, and the scene is done only when the Send button is
enabled again, so that the recording shows the complete text and nothing is still computing
when the program exits.

## hover_tool

The tour names every tool in turn, so the pointer rests on each long enough for GTK to show its
tooltip; nothing is clicked.

## choose_view

The view menu holds radio items that open no dialog, so menu-fallback cannot tell from the dialog
watcher whether the keyboard reached the entry. The caller passes what the new view looks like
instead (a tree, a grid or the list exists), and the view is switched directly if that does not
show up.

## expanded-trees

The tree view starts collapsed at "Dieser Computer"; expanded, it shows the point of the view -
the synchronisations ordered by the folders of source and target.

## tour_active

wxGrid toggles a check box cell on a click only when that cell is already the current one; the
first click on another cell just moves the cursor there. The cursor is therefore set before the
pointer arrives, so that the one visible click toggles. Should it still not toggle, the value is
set through the grid event the harness uses, and the recording only lacks the effect of the
click.

## tour_delete

The delete confirmation is a native message box (utilwx.md#confirm). The tests answer it before
it is shown (ModalDialogWatcher.md#answer); here it stays open while the narration explains it,
and the pointer clicks its "Löschen" button (Pacing.md#click_message_button).
