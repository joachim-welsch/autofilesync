# GuiTestSandbox

Module comment for GuiTestSandbox.h / GuiTestSandbox.cpp.

## set_environment_variable

Both blocks are needed: the configuration reads its paths through the CRT environment
(_wdupenv_s), wxWidgets through the Win32 one.

## which-variables-matter

Config::getRootPathname() builds the configuration path from APPDATA and the path it migrates
from out of USERPROFILE; wxGetHomeDir() asks HOME first.

## homedrive-homepath

fsu::home_dir() builds the home directory from HOMEDRIVE and HOMEPATH on Windows, and the
recycle bin's default path hangs on it. Without these two the application under test
emptied into - and a test once emptied - the developer's real `~/.recyclebin`. HOMEPATH
carries the path without the drive and with a leading backslash, the way Windows sets it.

## minimal_config

Enough of a configuration to keep the start-up quiet: an unreadable or missing file makes
Config::read() pop up a message box before any test runs. The log path is written with forward
slashes - a Windows path with backslashes would be an invalid escape sequence for the JSON
parser.

## isolate_input_method

Under GTK the application under test would otherwise connect to the desktop's input method
daemon (ibus on GNOME), which then stands between the simulated keystrokes and the text field:
it translates them asynchronously with the input source it currently holds, and the test
process - with a sandboxed configuration and its own idea of an input source - could switch
the developer's keyboard layout through it. GTK's built-in context translates the keystrokes
itself, synchronously, with the layout the keys were looked up in (see UserActions.md). Set
before wxWidgets starts, because GTK reads the variable while it initialises.

## isolate_preferences

The window geometry and the help window's settings go through wxConfig, which does not follow
the redirected environment everywhere: on macOS wxStandardPaths asks the system for the
Preferences folder of the real home, on Windows wxConfig is the registry. A width saved by
an earlier run on a wider screen then put the main window partly beyond the screen edge, and
every click on the synchronisation list landed outside the display. A file in the sandbox
replaces the default before the application initialises, and wxWidgets deletes it with the
other modules at wxEntryCleanup(), before the sandbox is removed.

## config_file

The same place Config::get_root_pathname() derives from the redirected environment, per
platform: APPDATA on Windows, Library/Application Support under HOME on macOS,
XDG_CONFIG_HOME on Linux. A different path here makes every test that reads the saved file
look at a file the application never writes.

## GuiTestSandbox

Throw-away HOME/XDG_CONFIG_HOME for the whole test run, so a GUI test can never read or damage
the developer's real configuration. create() must run before wxWidgets starts: Config and
wxStandardPaths resolve their paths from the environment as soon as the application initialises.

## use_private_scope

Keeps the sync arbiter of the application under test away from a backgroundsync daemon of the
developer: with the shared lock names a GUI test's synchronisation waits until the daemon's pass
ends, which can take longer than the tests wait for a copy. See SyncArbiter.md#use_private_scope.
