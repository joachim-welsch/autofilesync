# Settings

Module comment for Settings.h / Settings.cpp.

## delete_mode_

Recoverable deletion is the safer default and also what a config file written before this
setting existed falls back to.

## backup_proof_path_

Directory the proofs of backup go into, one file per sync item. Empty means none are written,
whatever a single sync item asks for - there is nowhere to put them.

## reset

Console mode comes from the command line, not from the configuration file, so it survives a
reload of the configuration.

## show_errors_

Errors of a single run are shown in a box unless the user switches them off; warnings and
infos are off unless asked for. A user who never opens the settings should not miss an
unreachable source, but should not be shown every copied file either.
