# AutoFileSync

**English** | [Deutsch](README.de.md) | [Español](README.es.md) | [Français](README.fr.md) | [Italiano](README.it.md) | [Polski](README.pl.md) | [Українська](README.uk.md)

AutoFileSync keeps files in two places in sync: a single file, a directory or
a whole directory tree. Each place can be a directory on your own computer, a
network drive, a USB drive, a computer reachable over SSH or an Android phone.
Set it up, start it, forget it.

AutoFileSync is free software under the GNU General Public License, version 3
or later, and runs on Windows, macOS and Linux.

Website: <https://welsch-software.com>
Source code and bug reports: <https://github.com/joachim-welsch/autofilesync>

## Features

- Synchronisation of files, directories and directory trees, narrowed down
  with wildcards.
- Comparison by date, size or content; move instead of copy; configurable
  handling of deleted files and of newer files in the target.
- Two-way sync that remembers the last state: deletions and renames are
  carried over, and files changed on both sides are kept as conflict copies.
- A preview that shows what a run would do before anything changes.
- Nothing gets lost: generations keep older versions, deletions go to the
  trash, a disconnected drive is never taken for "everything deleted", and
  mass deletions only run after they have been confirmed in the preview.
- Source or target over SSH on a NAS, a server or a computer in your home
  network; passwords are stored encrypted.
- Android phones are paired with a QR code; templates back up photos to the
  PC, keep a folder in sync or put music on the phone. The app
  (`AFS_Android/`) syncs in the background, optionally only on Wi-Fi and while
  charging.
- Changes are picked up at once: local directories are watched instead of
  only being checked at fixed intervals.
- A background service that syncs with the same configuration, even when no
  user is logged in.
- A built-in assistant that answers questions about the program from the
  help, explains why a target is not up to date and suggests matching
  commands.
- A log with selectable level, e-mail notification on serious errors and a
  backup report in JSON format.
- User interface and help in English, German, Spanish, French, Italian,
  Polish and Ukrainian; the language can be chosen in the settings.

## Installation

Ready-made packages for Windows, macOS and Linux are available at
<https://welsch-software.com>. To build it yourself, see `doc/BUILD.md`; the
complete setup including the background service is described in
`doc/INSTALL-Linux.md` and `doc/INSTALL-macOS.md` (these documents are in
German).

Quick start on Linux:

```bash
sudo apt install build-essential cmake git pkg-config libwxgtk3.2-dev \
                 libboost-filesystem-dev libssh-dev libssl-dev imagemagick gettext
scripts/build.sh
scripts/run_autofilesync.sh
```

## Structure

| Directory | Contents |
|---|---|
| `AFS_Core/` | Synchronisation engine as a static library, without user interface |
| `AFS_Desktop/` | Program with wxWidgets user interface, help and installer templates |
| `backgroundsync/` | Background service for Linux, macOS and Windows |
| `AFS_Android/` | Android app (Kotlin), build instructions in `AFS_Android/README.md` |
| `po/` | Translation catalogues |
| `third-party/` | Licence texts of the libraries used |

Tests: `scripts/run_Integrationtests.sh` for the engine,
`scripts/run_GUI_Tests.sh` for the user interface; details in `doc/BUILD.md`.

## Contributing

Bug reports and feature requests go to
<https://github.com/joachim-welsch/autofilesync/issues>, changes come as pull
requests. Contributions are licensed under the same licence as the project
(`doc/LICENSING.md`). Code style and workflow are described in
`doc/CONTRIBUTING.md`.

## Support

AutoFileSync is and remains free of charge. If you would like to support its
development, you can donate via PayPal to `joachim.welsch@posteo.de`. Custom
extensions and adaptations are gladly implemented for a fee; please send
enquiries to `contact@welsch-software.com`.

## License

GNU General Public License v3 or later, see `LICENSE` and `doc/LICENSING.md`.
Copyright © 2026 Joachim Welsch. Third-party components and their licences
are listed in `doc/THIRD-PARTY-NOTICES.md`.
