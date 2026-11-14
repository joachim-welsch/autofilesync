# ModelLocation

Module comment for ModelLocation.h / ModelLocation.cpp.

## model_location

Decides which GGUF file the assistant chat loads. Kept apart from LlamaClient so the lookup can
be tested without linking llama.cpp.

## directory

The per-user models directory: `%LOCALAPPDATA%\autofilesync\models` on Windows,
`~/Library/Application Support/autofilesync/models` on macOS (where setup_llama_macos.sh puts
the download) and `$XDG_DATA_HOME/autofilesync/models` (default `~/.local/share`) on Linux.
Empty if the environment names no home at all.

## find

AUTOFILESYNC_LLAMA_MODEL wins when it is set, even if the file does not exist: someone who
points it somewhere means that file, and silently loading a different model from the directory
would hide the mistake. Otherwise the directory is searched on every call, not cached, so a
model dropped in while the program runs is found without a restart. Empty if neither yields a
file.

## local-not-roaming

LOCALAPPDATA, not APPDATA like the configuration: a model is several gigabytes, and a roaming
profile would copy it to the server at every logoff.

## partial-downloads

Only the exact extension counts: the setup scripts download to `<name>.gguf.part` and rename
on completion, so a download still in progress is never picked up half-written.

## first-by-name

Several models in the directory: the first by file name, so the choice is the same on every
call instead of depending on the directory order of the file system.
