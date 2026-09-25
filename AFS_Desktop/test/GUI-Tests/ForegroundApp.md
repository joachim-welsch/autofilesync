# ForegroundApp

Module comment for ForegroundApp.h / ForegroundApp.mm (macOS only).

## bringToForeground

macOS delivers keyboard events to the active application and lets only its windows become key.
A test binary started from a terminal is not necessarily activated - which application is
active after the start depends on what was active before, and a click on a window of an
inactive application does not reliably activate it while a modal loop is running - so every
dialog then stays inactive: the clicks land, but no field takes the focus and the typed text
goes nowhere. activateIgnoringOtherApps is the call that makes this process the active one, the
same as a user clicking its Dock icon; the "ignoring others" part is what lets a background
process take the activation away from the terminal.
