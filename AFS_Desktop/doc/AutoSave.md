# AutoSave

Module comment for AutoSave.h / AutoSave.cpp.

## autosave

The configuration is written to its file right after every change - a tick in the grid, a
dialog left with OK, a deleted item - instead of waiting for "Datei > Speichern". Users did not
expect a separate save step: an item disabled in the dialog but not saved was still synchronised
by the background service, which only ever reads the file. Every GUI site that mutates Config
calls after_change() once it is done, so the file never lags behind what the window shows.

## save

Writes the configuration to its own path and tells the user when that fails: a failed save -
disk full, permissions - must not pass silently, because the window would then show a state the
file does not have. Config::changed() stays set after a failure, which is what the title marker,
the enabled "Speichern" command and the questions before closing or starting the service key on.

## after_change

Saves only when Config reports a change, so a dialog cancelled or left without edits does not
rewrite the file - and does not wake the background service, which reloads on every write.
