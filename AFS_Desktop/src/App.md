# App

Module comment for App.h / App.cpp.

## guitest-entry-point

The GUI test binary brings its own main() (Google Test); apart from the entry point the
application under test is built exactly like the shipped one.

## messageCatalogueDir

Same two places as the help book: next to the executable in a build tree or an installation,
among the resources inside the macOS bundle.

## initLocale

The language comes from the environment, not from a hardcoded one: setlocale(LC_ALL,
"de_DE.UTF-8") used to pin German here, which silently did nothing on a system where that locale
was never generated - setlocale then returns NULL, the locale stays "C", and gettext skips
translation entirely. German is only the fallback for a session that names no language at all;
see Translation.md#init_locale for why that fallback has to exist.

## language-from-environment

gettext takes the language from LANGUAGE/LC_MESSAGES, so a translated run needs no rebuild:
LANGUAGE=en autofilesync.

## log-single-instance

wxLogError alone is easy to miss headlessly (a cron/systemd --console trigger with no display
attached) - log it to the app's own log too, now that the log path actually resolves to
somewhere real by default (see the Logging default-path fix).
