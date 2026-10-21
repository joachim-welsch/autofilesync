# DemoSandbox

Module comment for DemoSandbox.h / DemoSandbox.cpp.

## DemoSandbox

The counterpart of GuiTestSandbox for a recording: a private configuration, a readable photo
tree as the source and an empty "USB-Backup" directory as the target, with nothing of the
user's own configuration reachable. Unlike the test sandbox it is meant to be looked at, so
the names are those a viewer expects to see in the dialog; the photo folder is named by the
recording (Options.md#names) so that it matches the interface language.

## redirect_user_directories

HOME and XDG_CONFIG_HOME point into the sandbox so that the application reads the demo
configuration. XDG_DATA_HOME is pinned to the real data directory first, because the
assistant looks for its language model below it (ModelLocation.md) and the model is not
part of the sandbox.

## demo_config

Message boxes for warnings and infos are off: a box would stop the scenes, and the narration
explains what happens instead. Errors stay on because they would be a reason to stop the
recording. MinimizeInsteadOfClose is on for the scene that closes the window while the
synchronisation keeps running.

## photo-sizes

Deterministic sizes between 150 and 450 KB: large enough that the progress dialog shows the
copy, small enough that the first run of 24 files takes seconds, and the same on every
recording so that two takes can be cut together.

## isolate_preferences

The window geometry is written into the preference file before the application reads it, so
that the main window opens centred with the size the recording expects instead of where the
developer last left it.

## new_photo

Kept outside the source tree until the scene copies it in: that is the moment the narration
calls "a new photo arrives".
