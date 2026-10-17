# ForegroundApp

Module comment for ForegroundApp.h / ForegroundApp.mm (macOS only).

## bring_to_foreground

macOS delivers keyboard events to the active application and lets only its windows become key.
A test binary started from a terminal is not necessarily activated - which application is
active after the start depends on what was active before, and a click on a window of an
inactive application does not reliably activate it while a modal loop is running - so every
dialog then stays inactive: the clicks land, but no field takes the focus and the typed text
goes nowhere. activateIgnoringOtherApps is the call that makes this process the active one, the
same as a user clicking its Dock icon; the "ignoring others" part is what lets a background
process take the activation away from the terminal.

## window_at_point

Asks the window server for its on-screen windows, front to back, and takes the first one that
contains the point: that window receives a click there. Windows above the modal panel level -
menu bar, Dock, the cursor - and fully transparent ones are skipped, so that only something a
user would see as covering counts. The window number identifies it; a window of this process
is named by the title of its NSWindow, a foreign one by its owning application. Window numbers,
bounds and owners come without the screen recording permission, which only the names of foreign
windows would require.

## window-number

The target is identified by its window number, never by the NSWindow or the wx object: the
check pumps events while it waits for the raised window, and a progress dialog whose copy
finishes meanwhile is destroyed under it - touching the wx object afterwards crashed the run.
The number stays valid as a key, and window_exists tells whether AppKit still has a window
for it, before anything is done to the wx object again.

## settle_window_server

Runs the native Cocoa run loop for a few short cycles. macOS commits window ordering to the
window server only when the run loop runs at the top level; a window shown shortly after another
top-level window was destroyed, with no such cycle between, never reaches the server and no later
window of the process does either (ForegroundApp.md#window_at_point). The modal dialogs are created
and destroyed per use, and over a long run that churn accumulates into the stuck state; a clean
cycle between tests clears it. Called from GuiTest::TearDown, which runs at the top level between
tests, not nested in the event loop where the cycle would have no effect.
