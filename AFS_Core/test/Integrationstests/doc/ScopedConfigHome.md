# ScopedConfigHome

Module comment for ScopedConfigHome.h.

## ScopedConfigHome

Points the configuration directory (Config::get_root_pathname) into a test directory and
restores it when the guard ends. Each platform derives that directory from a different
variable - APPDATA on Windows, HOME on macOS, XDG_CONFIG_HOME on Linux - so redirecting only
XDG_CONFIG_HOME let tests on Windows and macOS write into the user's real configuration
directory. config_dir() is the directory Config then uses, the parent of the configuration
file and of the state directory.
