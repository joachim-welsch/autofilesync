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
