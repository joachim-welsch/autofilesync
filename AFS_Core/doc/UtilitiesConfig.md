# UtilitiesConfig

Module comment for UtilitiesConfig.h.

## config

What the general-purpose modules in Utilities have to know about the program they are compiled
into. Every header of Utilities includes this file by name and every program that compiles
Utilities supplies its own, the same way it supplies pch.h; nothing else of AFS_Core is visible
there. The values live in the namespace Utilities::config, because that is where the modules
look for them.

## namespace

The modules of Utilities have their own namespace. The using-directive makes it part of AFS_Core,
so the code of the program writes util::trimmed() or log_error() as it writes the names of the
core itself, and AFS_Core::Logging still names the class. It stands here and not in every file
that uses a helper because this header reaches each of them: a file cannot include a module of
Utilities without it. Outside of AFS_Core the names are spelled Utilities::... .

## APPLICATION_NAME

The technical name of the program: Logging uses it for the directory and the file name of the
default log file (Logging.md#get_default_log_file_path), Translation as the gettext text domain,
that is the name of the compiled catalogue <language>/LC_MESSAGES/<name>.mo (Translation.md#init).
Lowercase ASCII without blanks, because it becomes part of paths.
