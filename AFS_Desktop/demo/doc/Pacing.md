# Pacing

Module comment for Pacing.h / Pacing.cpp.

## Pacing

What makes the demo watchable: the GUI tests move the pointer instantly and type as fast as
the X server takes it, a viewer needs motion they can follow. Everything here wraps the
UserActions primitives of the test harness with time.

## set_speed

One factor for every pause and movement. A rehearsal runs fast, the recording at 1.0; the
scenes do not know the difference.

## glide_to

The pointer travels along a straight line with a smoothstep profile - slow start, slow
arrival - in 16 ms steps, which a 30 fps recording shows as continuous motion. The last
MouseMove lands exactly on the target so that the following click_on, which moves there
again, causes no visible jump.

## glide_click

Glide first, then the harness's click_on: the click is the same one the tests make, so the
click observer (UserActions.md#set_click_observer) sees it.

## press_key

wxUIActionSimulator::Char for navigation keys only. Text goes through the harness's keyboard
(UserActions.md#why-not-simulator-text); the menu bar, arrows, Return and Escape are
layout-independent.

## type_slowly

About 90 ms per character reads as a person typing. The delay is set on the harness for the
one call and cleared again, so a later harness call is not slowed down.

## tool_rect

wxToolBar has no portable "rectangle of tool x". On GTK, FindToolForPosition is not even
implemented (it asserts), so the rectangle comes from GTK itself: the n-th tool item of the
GtkToolbar that wxToolBar::GTKGetToolbar() exposes, with n = GetToolPos(id), since both lists
count separators and spacers alike. The three GTK functions are declared by hand because the
GTK headers are not a build requirement of the project, only the library wxGTK links anyway.
On the other ports FindToolForPosition answers the inverse question, and scanning the
toolbar's middle line in 2 px steps gives the horizontal extent of the tool.

## tab_rect

Same idea for notebook tabs with HitTest: the label area of the page is scanned in the top
quarter of the notebook, where the tab row is on every platform the demo runs on.

## message_button_center

wxMessageDialog on GTK is a native GtkMessageDialog: its buttons are no wxWindows, so neither
find() nor the harness's click_on reaches them. GTK knows the button for a response id; its
allocation, translated to the dialog and offset by the dialog's GdkWindow origin, is the screen
point a person would click. The widget exists only once ShowModal has created it, which is why
the scenes ask from inside a ModalDialogWatcher action (queued, ModalDialogWatcher.md#queued-not-called).

## click_message_button

A real click through the XTest simulator, so that the button shows its pressed state in the
recording, and a click sample for the telemetry, which the harness's click observer does not
see because the click does not go through click_at. If the button cannot be located the
response is sent to the dialog directly - the recording then lacks the click, but the scene goes
on.
