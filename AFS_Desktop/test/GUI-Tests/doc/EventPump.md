# EventPump

Module comment for EventPump.h / EventPump.cpp.

## pump

wxUIActionSimulator hands its events to the X server, so nothing happens until the application's
event loop gets a chance to read them back.

## idle-time-not-wall-time

The time is counted in idle steps, not on a stopwatch: a single wxYield() can run for seconds
when a handler it dispatches opens a modal dialog and the test drives it to the end from
inside, and a stopwatch would then declare the pump over after that one yield - the click that
opened the dialog got no settling time at all. Counted this way, pump(20) always gives the
application the same few cycles after the handler has returned, however long it ran.
