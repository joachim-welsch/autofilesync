# Recording

Module comment for Recording.h / Recording.cpp.

## Recording

The demo's clock and its two outputs for the editor: scene marks with time stamps and the
cursor telemetry. Both are relative to the moment start() is called, which the recording
script places right after ffmpeg has begun to capture, so the stamps match the video to a
fraction of a second.

## mark

One line per scene event, "mm:ss.t  text", on stdout and in the markers file. The editor
reads it next to the timeline to find the cuts the narration asks for, such as the wait for
the synchronisation cycle.

## telemetry-format

Recordly's cursor sidecar: <video>.cursor.json with version 2 and samples of
timeMs, cx, cy (0..1 of the recorded screen), interactionType and cursorType. Recordly reads
the file when the video is imported and uses it for the rendered cursor, click effects and
auto-zoom suggestions. Positions are sampled every 33 ms from a timer, which fires while
the demo pumps events; clicks come from the harness's click observer as a click followed by a
mouseup, the way Recordly's own capture records them.

## finish

Writes the file in one go at the end: a recording is short, and a partial file would be
mistaken for a complete one.
