# UserActions

Module comment for UserActions.h / UserActions.cpp.

## why-not-simulator-text

wxUIActionSimulator::Text() accepts letters, digits and " +-.," only, which rules out the path
separators every test path needs. Char() takes a single character, but what it expects there
differs per platform.

## windows-injects-the-character

The character is injected directly instead of through a key of the active layout. Going through
keys means going through the layout's dead keys: on a German keyboard '`' and '^' produce
nothing until the following key decides what to combine into, so a value ending in one loses it
- and the pending character then turns up in whatever is typed next, in the next test. The
layout also decides which modifiers a character needs (AltGr for '[', '\', '@' among others),
and none of that matters with an injected character.

## macos-injects-the-character

The character is injected as a unicode string instead of through a key of the active layout -
same reasoning as the Windows branch above. Char() maps to the key codes of a US layout, which
any other layout then reinterprets: on a German one 'y' and 'z' swap places and every character
sitting on a different key ('-', '/', '_', '.') is dropped altogether, so names and paths
arrived mangled ("autofilesync-guitest" as "autofilesznc guitest"). Posted to this process
instead of the HID tap: the keystrokes reach the application under test without it having to be
the frontmost application, and cannot land in whatever else is open on the developer's desktop.

## macos-modifier-flags

wxMOD_CONTROL is the Command key on macOS, as it is for wxKeyEvent::ControlDown() there, and
wxMOD_RAW_CONTROL the Control key - the same mapping the application's key handlers see.

## press_return

The Return key goes the same way as the characters on macOS: wxUIActionSimulator::Char() posts
it to the HID tap, where it goes to whatever application is frontmost, and the queued
characters posted to this process overtook it - a two-line entry arrived as one line, and
Cmd+Return in the chat as a plain line break. Posted to the process with its modifier flags
set on the event itself, the key lands in the same field as the text before it, in order.
The other platforms keep the simulator, whose Char() presses the modifier keys for them.

## x11-keymap-lookup

The character is typed through XTest with the key that carries its keysym in the server's
current layout, together with the modifiers that reach that level of the key - Shift for '$'
and each capital, AltGr for '@', '[' or '\' on a German layout, both for some. Char() of
wxUIActionSimulator finds the key too but never presses a modifier by itself, so every
character above the first level arrived as the one below it; Text() only accepts characters
that need no modifier on a US layout. Which key, which level and which modifiers is what the
layout decides, so all three are read from it: the keysym table of each key (XkbKeySymEntry),
the key type's map from level to modifier mask, and the modifier map that names a key for each
modifier bit. Own connection to the same server the application uses: only the keyboard is
read and driven, and asking X directly avoids pulling GTK's headers in.

## keyboard-per-text

The layout is read afresh for every text, not once per process: Xlib refreshes its copy of the
keymap only while a connection processes events, which this one never does, and the desktop
can switch the layout at any time - one run started on the session's German layout and found
it switched to US while it typed: the stale German copy asked for Shift on a key that no longer
needed it, so every '/' arrived as '?'. A run that starts on one layout and ends on another
types correctly on both.

## keysym-of-character

The keysyms of Latin-1 are its code points, everything above is the code point with the Unicode
keysym bit set - the same mapping XKB uses for its own symbol tables.

## x11-dead-keys

A layout may carry a character only as a dead key: '^' and '`' on a German keyboard produce
nothing until the following key decides what to combine into. Typed as the dead key followed by
a space, which is how a user gets the plain character, and what GTK's own input context
composes it to.

## x11-level-modifiers

The modifiers of a level come from the key's type, not from a fixed table: a two-level key
reaches level 1 with Shift, a four-level key its level 2 with AltGr, and an alphabetic key lists
Lock for level 1 as well - the entry found first is the Shift one, which is the one to press.
A level without an entry cannot be reached by any modifier, so the search moves on to the next
key that carries the keysym.

## x11-event-spacing

Fake events sent too fast get lost between XTest, the compositor and GTK; the pause is the
one wxUIActionSimulator keeps between its own events for the same reason.

## click-until-focused

A text field is clicked again, at its current position, when the focus has not arrived: a
window that moves between the reading of the position and the click - a dialog that GNOME
re-centres on a parent the window manager has just resized - takes the click somewhere else,
and the text would then go into whatever sits there. After the attempts the typing goes ahead
anyway: on some platforms the focus is reported late although the click landed, and the value
check afterwards tells.

## raise-when-not-focused

A click that leaves the field without focus is followed by a request to activate its window.
GNOME withholds the focus from a window the application shows without an action of the user:
the main window a test hid and showed again, and every dialog opened above it afterwards, are
marked as demanding attention instead - no window of the application is active, and the fake
click on the field does not change that. Raise() asks the window manager for the activation
with the time of the input the window did receive, which is what a user's click provides.

## retype-once

A text is typed a second time when the field does not show it afterwards: a keystroke can get
lost on the way, and a layout switch in the middle of the text produces a wrong character once
- both are retried with a freshly read layout before the failure is reported. Selecting the old
content first makes the second attempt replace the first.

## wait-until-settled

A window manager places a dialog after it has been mapped: GNOME pushes it down under its
title bar about 20 ms after IsShownOnScreen() turns true. A point computed before that lands
19 pixels too high - one row up in a list, on the label above a text field - and the click
selects nothing or types into nothing. Every click therefore waits until two readings of the
window's screen position agree. On Xvfb, without a window manager, the first pair agrees and
the wait ends at once; the timeout only guards against a window that keeps moving.

## pointer-arrives-before-click

The click is sent only once the pointer is reported at the target. On a Wayland desktop the
fake motion travels through the compositor before the server knows the new position, and a
click sent a fixed 20 ms after it sometimes went to the previous position - into the field
typed before, which then swallowed the next text. Xvfb moves the pointer at once. The timeout
covers a screen where the reported and the requested position never agree (scaling); the click
is then sent anyway, as it was before.

## pointer-move-settles-focus

Moving the pointer first also settles the keyboard focus: on an X server without a window
manager the focus follows the pointer, so a key event sent to a control the pointer never
visited would end up elsewhere.

## caller-ensures-visibility

Only the item's row needs to be visible, not the tree itself scrolled to it: EnsureVisible()
before this call is the caller's responsibility.

## select-after-the-click

Selected after the click, not before: the click sets the caret and drops an earlier selection,
so the new text would land in the middle of the old value instead of replacing it.

## select_radio_option

wxRadioBox is a single window: on MSW its buttons are internal children that wx does not expose,
so there is nothing to point the mouse at. Selecting and sending the command the click would
produce runs the same dialog handler.

## update-ui-needs-idle

Buttons guarded by EVT_UPDATE_UI only become enabled while the event loop is idle, which is
exactly what pumping inside wait_until() provides.

## activate_menu_item

wxUIActionSimulator cannot drive a menu bar reliably - the menus are native popup windows whose
position wx does not expose on either platform - so the command takes the same event table entry
that a real click would reach.

## UserActions

User level actions on top of wxUIActionSimulator: every helper is null safe and reports failure
instead of aborting, so a test states what went wrong.

## click_on-tree-item

Clicks the tree item's on-screen position, the way a user would - unlike
wxTreeCtrl::SelectItem() this goes through the real mouse event handlers
(TreeCtrl::on_mouse_left_down and whatever it triggers).

## replace_text

For a field that already holds a value: selects it first, the way a user overwrites an entry
instead of appending to it.

## find_tree_item_for_path

Walks the tree the same way TreeCtrl::add_branch() built it: one child node per path segment, the
filesystem root itself excluded. Invalid if any segment has no matching node.

## select_entry

wxChoice opens a native popup list whose entries the simulator cannot locate, and wxListBox
exposes no portable item rectangle, so the entry is selected directly and the command the
selection would produce is sent - the same handler as for a real pick, see select_radio_option.

## set_check_box

A real click, because the dialog's rules (two-way sync locking "copy when target newer") hang
on the click's command event. Only the difference to the wanted state is clicked, so the
helper is idempotent. A disabled box reports failure: clicking it would silently do nothing.

## click-the-box

Clicked on the box itself, not on the centre of the control: the sizers stretch a check box
over the whole dialog width, and macOS reacts only to clicks on the box and its label - a
click in the empty space right of a short label ("Aktiv", "Start minimiert") changes nothing.
The box sits at the left edge and is as wide as the control is high.

## click-arrives-later

On macOS the simulator hands the click to the window server, which delivers it to the
application a moment later; the value read right after the pump was still the old one for
every second box in a row. The value is waited for instead, up to the click timeout - a click
that has already arrived costs no time.

## settle-after-dialog

A click whose handler ran a modal dialog - the ssh button of the synchronisation dialog opens
the account selection and the remote directory - returns to its caller only when those dialogs
have closed, and AppKit re-orders the windows of the application a moment after that: the main
window comes to the front over the dialog the button belongs to. A dialog that is ended before
that has happened leaves the re-ordering pending, and it then lands on the next dialog the test
run opens, which sits behind the main window from then on - every click aimed at it hits the
main window instead. The idle time after such a click lets the re-ordering happen while the
dialog it concerns is still there.

## click_on-point

The variant with a client point is for controls whose centre is the wrong spot; the pointer
version keeps clicking the centre.

## set_spin_value

Typing into a wxSpinCtrl differs per platform (the text part is an internal child on MSW), so
the value is set and the spin command sent instead, like select_radio_option.

## show_notebook_page

Controls on a page that is not shown are not on screen, and clicking them fails. The page is
switched through the notebook itself: the dialogs do not react to page changes, so nothing is
skipped by not clicking the tab.

## activate_toolbar_tool

wx exposes no cross-platform rectangle of a toolbar tool to click on, so the command a click
would produce is sent through the toolbar's handler - wxEVT_TOOL is what the click ends up as.
The tool has to exist and be enabled, so a greyed-out tool fails the way it would for a user.

## replace_lines

Line breaks go in as the Return key, not as a character: an injected '\n' is not a key press
and a multi-line wxTextCtrl ignores it. The field must be multi-line, a single-line one would
swallow the Return as "OK".

## click_on-list-item

Clicks the row's on-screen rectangle, so the list's own selection handling and - with a
double click - ListCtrl::on_double_click run the way they do for a user.

## click_on_row_label

The row labels live in their own child window of the grid, positioned in scrolled
coordinates; the cell rectangle gives the row's height and unscrolled top, which is scrolled
here. A click on the label is what opens the edit dialog in the grid view.

## change_grid_cell

In-place editing differs per platform and cell editor (the bool editor toggles on a click,
the text editor needs Enter to end), so the value is set and the "cell changed" event sent -
the same handler that ends a real edit, GridCtrl::on_cell_value_changed.

## click-must-reach-us

On macOS the simulated click goes to whatever window the window server finds at the pointer,
and that is not always the dialog the control belongs to: AppKit re-orders the application's
windows a moment after a modal dialog has ended (settle-after-dialog), and a dialog opened
right then - the progress dialog that "synchronise once" shows straight after the item dialog -
comes up behind the main window. A click aimed at it hit the main window instead, which left
every dialog of the rest of the run behind the main window, so that each typed text and each
OK went astray; a terminal the user clicks into covers a dialog the same way. Before the click,
the window at the target point therefore has to be the control's own top-level window. Any
other window gets one attempt at activating and raising the target; then the click is refused
with a failure that names the target and what lies in front of it, so the test fails for the
visible reason, and no text is typed into another application's window. Under Xvfb no window
covers another, and on Windows the check is not implemented.

## window-number-not-pointer

Everything the check needs from the dialog - its window number and its title - is read before
the first wait, and after a wait only the number is used: the dialog may be gone by then
(ForegroundApp.md#window-number), and the failure then says so instead of crashing.
