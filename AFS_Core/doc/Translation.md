# Translation

Module comment for Translation.h / Translation.cpp.

## translation

GNU gettext binding of the sync core. The message ids are technical keys
(source_dir_not_reachable), not display text: no language is privileged, and rewording a message
in one language touches neither the code nor the other catalogues. The price is that a run
without a catalogue has no text to fall back on and shows the key - see no-libintl-fallback.

## init_locale

The language of the messages comes from the environment, which is what gettext reads - but only
while the LC_MESSAGES locale is not "C": there it refuses to translate, and with technical
message ids every message would come out as its key. A session that names no language at all
therefore gets the first of the caller's fallback locales that the system accepts: setlocale()
returns NULL for a locale that was never generated, and a single "de_DE.UTF-8" then left the
process in "C" after all. DEFAULT_LOCALES names German in decreasing strictness, from the
UTF-8 locale over the plain one to the bare language; autofilesync and backgroundsync both pass
it, so that the two programs of one installation cannot drift apart.

Called before init(): binding the catalogue does not re-read the locale.

## locale_dir

The "locale" directory beside the running executable, which is where the build puts the compiled
catalogues (see the msgfmt block in the top-level CMakeLists.txt) - empty if it is not there, so
an installed program falls back to the system search path below. Found from the executable
itself, not from argv[0] or the working directory: a service is started with whatever working
directory its unit file happens to have.

## executable_path

Every platform needs its own call for this, and none of them is std::filesystem: /proc/self/exe
is Linux-only, _NSGetExecutablePath() is what the loader on macOS offers, and
GetModuleFileNameW() may need more than MAX_PATH for a long path. All three report failure as an
empty path, which locale_dir() turns into "use the system search path".

## init

Binds the catalogue "autofilesync" below localeDir, where gettext expects
<localeDir>/<language>/LC_MESSAGES/autofilesync.mo. An empty path leaves the system search path
in place - callers cannot always locate the directory.

## fall_back_to_english

A session in a language without a catalogue (sv_SE, say) would see every message as its key,
because the ids are technical. gettext has no fallback language of its own, but it honours a
list in LANGUAGE; when no catalogue answers the empty msgid - which every catalogue maps to its
header - the language list is replaced by "en". Checked by init() and by every set_language().
Does not help a process stuck in the "C" locale, where gettext ignores LANGUAGE; init_locale()
avoids that wherever the system has a German locale.

## LANGUAGES

The catalogues shipped in po/, each with the name its own speakers use, so that a user who
cannot read the current language still finds theirs. A new po/<language>.po needs an entry here
to become selectable in the settings.

## environment_language

LANGUAGE as init() found it, before the English fallback or a chosen language overwrote it:
what "the operating system's language" means when the user withdraws a choice. nullopt when
the environment had no LANGUAGE, which then must be removed again, not set empty.

## set_language

Switches the language of every later translate() call, by the same LANGUAGE variable gettext
reads; an empty code returns to environment_language. textdomain() is called again because
it bumps gettext's catalogue counter, which drops translations cached for the old language.

LANGUAGE is process-wide and setenv() is not thread-safe against a concurrent getenv(): a
sync thread translating a message while the user switches the language could race. The window
is a single user action, and the old string the variable pointed to stays valid on glibc.

## msgid

Returns its argument unchanged; it only marks a message id that is stored now and translated
later (a lookup table, a prompt assembled at call time) so that xgettext finds it. Without it
those ids reach no catalogue and every language shows the bare key.

## translate_utf8

The narrow variant for the message paths that carry std::string: the exception texts the SSH
modules throw and SshDirInspector's error_message. They are UTF-8 like the catalogue, so this
hands the catalogue's bytes straight on instead of converting to std::wstring and back.

## placeholders

%1 .. %9 in the translated text stand for the values passed alongside the message id, so a
translation can put them where its own word order needs them - the reason a value that sits
inside a sentence is passed this way instead of being concatenated around two message fragments.
A value that simply ends the message is still concatenated by the caller.

Substituted left to right in one pass, so a value that itself contains "%2" is not substituted
again, and a placeholder without a value stays visible instead of swallowing the text around it.

## catalogue-codeset

The catalogues are UTF-8 and util::string_to_wstring() decodes UTF-8; without this gettext would
recode to the charset of the current locale.

## dgettext-instead-of-gettext

dgettext() instead of gettext(): the core must not depend on whichever domain the hosting
program set last.

## missing-translation

A translation that is missing is logged once per message id, as a warning with the id: the
bare key is what the user sees, and until now nothing said why - a program copied over an
installation whose catalogues stayed behind looked like a model that ignores its prompt. Only
while a catalogue is bound (its header translates), checked at that moment and not cached: a
test binary without catalogues, or one that just restored the C locale, would otherwise warn
about every message.
Logging translates its own messages ("log_message_repeated"), so the report guards against
re-entering itself on the same thread.

## catalogue_language

The language of the catalogue gettext actually bound, read from the "Language:" field of its
header (the translation of the empty msgid) - not the language that was asked for, which may
have fallen back to English (fall_back_to_english). The help picks its book by it
(MainWindow.md#help_language), so that menu texts and help speak the same language. Empty when
no catalogue is bound or the program was built without libintl.

## catalogue_mismatch

The message, if the catalogues gettext found were not built with this program: every catalogue
carries the stamp of the message ids it was compiled from (see cmake/CatalogueStamp.cmake), the
program the stamp of the ids it was compiled with. The directory named is the one gettext
searches, asked back from bindtextdomain(): with the GUI installed to /opt and the catalogues
in /usr/share/locale, the user would not guess which of the two is meant. Empty when the
program was built without a stamp (the Visual Studio projects) or no catalogue is bound at all
- then there is nothing to compare, and the bare ids are the known no-catalogue case.

## english-safety-net

The one message with a built-in English fallback: it reports that the catalogue is outdated,
and an outdated catalogue is exactly one that may lack this message. Everywhere else the ids
are keys without a fallback language (see no-libintl-fallback).

## no-libintl-fallback

The branch for builds that define no AFS_WITH_GETTEXT hands the message id straight back, so the
user reads "source_dir_not_reachable" instead of a sentence: message ids are keys, and there is
no longer a language to fall back on. The CMake build cannot end up here - it requires Intl -
but the Visual Studio projects still compile without gettext and do.
