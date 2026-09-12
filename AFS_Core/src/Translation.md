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
therefore gets the caller's fallback. The fallback is the calling program's to choose, not the
core's; both autofilesync and backgroundsync pass their own language, German.

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

## no-libintl-fallback

Builds without libintl (Windows without a gettext package) hand the message id straight back, so
the user reads "source_dir_not_reachable" instead of a sentence. Message ids became keys, so
there is no longer a language to fall back on; a build meant for users needs libintl and the
catalogues next to the binary (see locale_dir).
