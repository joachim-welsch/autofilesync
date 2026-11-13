# DaemonTests

Module comment for DaemonTests.cpp.

The worker of the background service (backgroundsync/MyDaemon.cpp), run inside the test
program: start, a pass over the configuration the GUI saved, reload after the GUI saved again,
stop. The entry points around it (backgroundsync.cpp, WindowsService.cpp) only start and stop
this class. Unix only, like the private home the configuration path relies on.

## DaemonTest

The daemon reads Config::get_root_pathname(), so the fixture moves the configuration home into
its own directory (ScopedConfigHome) and restores path, logging setup and language afterwards -
load_config() sets all three.

## PASS_TIMEOUT

The longest a test waits for a file to arrive. The daemon looks at the configuration once a
second and a continuous run rests between passes, so a few seconds are normal; the limit only
ends a run in which nothing arrives at all.

## configuration_text

The configuration as the GUI would save it, built with Config itself and read back as text.
It is written to the real path as plain text afterwards (save_like_the_gui), because the
daemon works on the same Config singleton as the test: writing through Config::write() while
the daemon runs would update the singleton's own record of the file time, and the change the
daemon is meant to notice would be invisible to it.

## DaemonSyncsTheItemsOfTheGuiConfiguration

Start, one copied file, stop.

## stopped-means-stopped

After stop() nothing may run any more. A file written afterwards has to stay where it is for
longer than a pass takes; that this needs a fixed wait is the price of proving an absence.

## DaemonReloadsAChangedConfiguration

The configuration is replaced by one that names another item. The new item's file arrives,
and the old item is no longer served: a file put into its source stays there while a file in
the new item's source - written after it - arrives, which proves that a whole pass went by.
