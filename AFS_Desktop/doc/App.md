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
see Translation.md#init_locale for why that fallback has to exist and how it survives a missing
locale.

## language-from-environment

gettext takes the language from LANGUAGE/LC_MESSAGES, so a translated run needs no rebuild:
LANGUAGE=en autofilesync.

## catalogue-check

Catalogues of another version (Translation.md#catalogue_mismatch) show bare ids in the GUI and
hand them to the assistant as its instructions, which then looks like a bad model. Checked
once the main window has read the configuration: the log file is opened by the configured
path, and a line logged before that is dropped. wxLogWarning queues the message until the
event loop runs, so startup goes on; the log line is for a headless (console mode) start.

## log-single-instance

wxLogError alone is easy to miss headlessly (a cron/systemd --console trigger with no display
attached) - log it to the app's own log too, now that the log path actually resolves to
somewhere real by default (see the Logging default-path fix).

## wx-locale

wxLocale gives wxWidgets' own texts - the stock buttons, the file dialogs, the command line
parser - the language of the session, from the environment like the message catalogue below.
It used to be pinned to German and to live only until the end of OnInit(), which had two effects:
while it lived, its setlocale() overrode the session's language for everything MainWindow built
in its constructor, and when it died, wxWidgets fell back to English. A member lives as long as
the application. It runs before init_locale(), whose German fallback for a session without a
language must be the last word on the C locale.
