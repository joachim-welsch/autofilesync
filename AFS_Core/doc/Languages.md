# Languages

Module comment for Languages.h.

## languages

The languages of this program. They stand in a namespace of AFS_Core and not in
Utilities::translation, which knows how to translate but not what a program ships.

## DEFAULT_LOCALES

The fallback locales for translation::init_locale() (Translation.md#init_locale): German in
decreasing strictness, from the UTF-8 locale over the plain one to the bare language.
autofilesync and backgroundsync both pass it, so that the two programs of one installation
cannot drift apart.

## LANGUAGES

The catalogues shipped in po/, each with the name its own speakers use. A new po/<language>.po
needs an entry here to become selectable in the settings.
