# SyncMessagesTest

Module comment for SyncMessagesTest.cpp.

## filter_messages

The three filters are set through the settings dialog, the way a user reaches them; the
sandbox configuration starts with all three off, so every test states what it wants.

## unreachable-source

A source directory that does not exist is the error a user runs into most; it is found in
the count pass, so the run ends in the "nothing found" box first and the collected errors
follow. The box has to name the source, otherwise the user does not know which
synchronisation failed.

## levels

One test per level with the message the run really produces: the empty source file the
sync refuses to copy (a warning) and the copied file (an info). The box titles carry the
highest level of what is shown, so a run with only infos is not presented as an error.

## continuous-is-silent

The continuous run is what a user leaves alone; a box every pass would defeat it, so the
filters apply to "Einmal synchronisieren" only. The unexpected-dialog list of the watcher is
the proof that nothing appeared.
