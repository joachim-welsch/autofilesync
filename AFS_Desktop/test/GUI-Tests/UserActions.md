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

## keyboardDisplay

Own connection to the same server the application uses: only the keyboard mapping is read, and
asking X directly avoids pulling GTK's headers in.

## x11-needs-the-layout

Char() looks the keysym up in the layout but never presses a modifier by itself, so every
character sitting on a shifted key - '$', '!', ':' and each capital - would arrive as the
unshifted character of the same key. Which characters those are depends on the layout, so the
layout decides here too.

## ascii-equals-keysym

ASCII code points and their X keysyms are identical, which is the same mapping Char() applies
before it looks for the key.

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

## selectRadioOption

wxRadioBox is a single window: on MSW its buttons are internal children that wx does not expose,
so there is nothing to point the mouse at. Selecting and sending the command the click would
produce runs the same dialog handler.

## update-ui-needs-idle

Buttons guarded by EVT_UPDATE_UI only become enabled while the event loop is idle, which is
exactly what pumping inside waitUntil() provides.

## activateMenuItem

wxUIActionSimulator cannot drive a menu bar reliably - the menus are native popup windows whose
position wx does not expose on either platform - so the command takes the same event table entry
that a real click would reach.

## UserActions

User level actions on top of wxUIActionSimulator: every helper is null safe and reports failure
instead of aborting, so a test states what went wrong.

## clickOn-tree-item

Clicks the tree item's on-screen position, the way a user would - unlike
wxTreeCtrl::SelectItem() this goes through the real mouse event handlers
(TreeCtrl::OnMouseLeftDown and whatever it triggers).

## replaceText

For a field that already holds a value: selects it first, the way a user overwrites an entry
instead of appending to it.

## findTreeItemForPath

Walks the tree the same way TreeCtrl::addBranch() built it: one child node per path segment, the
filesystem root itself excluded. Invalid if any segment has no matching node.
