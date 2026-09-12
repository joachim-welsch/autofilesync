# Translation

Module comment for Translation.h / Translation.cpp.

## translation

GNU gettext binding of the sync core. The message ids are the German source strings, so a
missing or unreadable catalogue falls back to exactly the text the program showed before it was
translated.

## init

Binds the catalogue "autofilesync" below localeDir, where gettext expects
<localeDir>/<language>/LC_MESSAGES/autofilesync.mo. An empty path leaves the system search path
in place - callers cannot always locate the directory.

## catalogue-codeset

The catalogues are UTF-8 and util::string_to_wstring() decodes UTF-8; without this gettext would
recode to the charset of the current locale.

## dgettext-instead-of-gettext

dgettext() instead of gettext(): the core must not depend on whichever domain the hosting
program set last.

## no-libintl-fallback

Builds without libintl (Windows without a gettext package) keep the German message ids - the
same texts the untranslated program used.
