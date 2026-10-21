# Options

Module comment for Options.h / Options.cpp.

## Options

Everything the demo takes from the command line. The defaults are the values the recording
script does not need to spell out: all scenes, a sandbox in the temp directory, three seconds
before the first scene.

## scenes

Names rather than numbers: the video script numbers its scenes by narration, the demo by what
happens on screen, and a name survives a renumbering of the script. The order on the command
line is the order of execution, so a single scene can be rehearsed on its own.

## root

The sandbox root is also what the viewer reads in the path fields of the dialog, so it can be
set to something that looks like a home directory instead of the temp directory.

## speed

A factor on every dwell, glide and typing delay (Pacing.md#set_speed): a rehearsal runs at
speed 3 and the recording at 1, with the same scenes.

## cycle_seconds

The synchronisation cycle of the item is 60 s by default. Left alone the demo waits that
long for the new photo to arrive, and the editor cuts the wait; set, the demo visibly puts
the cycle on the options tab, which the narration otherwise skips.

## names

The folder the viewer sees in the source field and the name typed into the dialog are
options because they are the only German words in a recording whose interface language
comes from LANGUAGE: a recording for the English video passes "Pictures" and "Photos" and
an English question, and the scenes stay the same.

## question-via-path

The question arrives as UTF-8 bytes; std::filesystem::path is the one standard conversion to a
wide string that follows the locale without a codecvt facet of its own.

## parse_options

Hand-written rather than wxCmdLineParser because it runs before wxWidgets is initialised: the
sandbox has to redirect HOME before wxEntryStart reads any preference file.
