# GuiTestSandbox

Module comment for GuiTestSandbox.h / GuiTestSandbox.cpp.

## setEnvironmentVariable

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

## minimalConfig

Enough of a configuration to keep the start-up quiet: an unreadable or missing file makes
Config::read() pop up a message box before any test runs. The log path is written with forward
slashes - a Windows path with backslashes would be an invalid escape sequence for the JSON
parser.

## isolateInputMethod

Under GTK the application under test would otherwise connect to the desktop's input method
daemon (ibus on GNOME), which then stands between the simulated keystrokes and the text field:
it translates them asynchronously with the input source it currently holds, and the test
process - with a sandboxed configuration and its own idea of an input source - could switch
the developer's keyboard layout through it. GTK's built-in context translates the keystrokes
itself, synchronously, with the layout the keys were looked up in (see UserActions.md). Set
before wxWidgets starts, because GTK reads the variable while it initialises.

## GuiTestSandbox

Throw-away HOME/XDG_CONFIG_HOME for the whole test run, so a GUI test can never read or damage
the developer's real configuration. create() must run before wxWidgets starts: Config and
wxStandardPaths resolve their paths from the environment as soon as the application initialises.
