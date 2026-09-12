# GuiTestSandbox

Module comment for GuiTestSandbox.h / GuiTestSandbox.cpp.

## setEnvironmentVariable

Both blocks are needed: the configuration reads its paths through the CRT environment
(_wdupenv_s), wxWidgets through the Win32 one.

## which-variables-matter

Config::getRootPathname() builds the configuration path from APPDATA and the path it migrates
from out of USERPROFILE; wxGetHomeDir() asks HOME first.

## minimalConfig

Enough of a configuration to keep the start-up quiet: an unreadable or missing file makes
Config::read() pop up a message box before any test runs. The log path is written with forward
slashes - a Windows path with backslashes would be an invalid escape sequence for the JSON
parser.

## GuiTestSandbox

Throw-away HOME/XDG_CONFIG_HOME for the whole test run, so a GUI test can never read or damage
the developer's real configuration. create() must run before wxWidgets starts: Config and
wxStandardPaths resolve their paths from the environment as soon as the application initialises.
