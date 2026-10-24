# MyDaemon

Module comment for MyDaemon.h / MyDaemon.cpp.

## MyDaemon

Runs the Synchronize engine in continuous mode over the configuration file of the user the
daemon runs as - the same file the GUI edits, so both synchronise the same items.

## reload_config

The daemon only ever sees the configuration through its file: the GUI holds its edits in memory
until the user saves, and a daemon that read the file once at start would keep synchronising
the old items for as long as it runs - a disabled item stayed active until the next restart.
The worker polls the file's modification time once a second (Config::file_changed_on_disk) and
reloads when it changed.

The engine is destroyed before the read and created afterwards: destroy() joins the worker
thread, which stops the running pass at the next item, so the new items are never mixed into a
pass that started with the old ones, and the fresh items start a full pass at once. Logging is
reopened as at start, because the logging setup is part of the file.

A file that fails to read leaves the configuration empty until it is saved again; Config::read
records the modification time before parsing, so the defective file is not re-read once a
second.

## catalogue-check

Catalogues of another version (Translation.md#catalogue_mismatch) are reported after the
configuration is read, in its language and into the log file it names - earlier the file is
not open yet and the line would be dropped. Repeated on every reload, which is when an
updated installation's catalogues would start to match again.

## device-link

The service listens for phones (DeviceLink.md) as soon as one is paired, so a phone is served
while nobody is logged in. Only one process of the user may listen; when the GUI holds the
role, the service tries again once a minute and takes over when the GUI quits. A phone paired
in the GUI meanwhile reaches the service through the configuration file, which the service
reloads when it changes. The link is stopped before the worker, because a pass the link waits
for needs the worker's synchronisation run.
