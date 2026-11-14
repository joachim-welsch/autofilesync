# CloudFolders

Module comment for CloudFolders.h / CloudFolders.cpp.

## cloud_folders

Finds the folders that the sync clients of cloud providers keep on this computer. Such a folder
is an ordinary local directory: AutoFileSync copies into it and the provider's own client
carries the files to the cloud. No account, no login and no network access is needed for that,
which is why this covers the large providers long before a real cloud protocol does.

## detect

Looks the folders up on every call and caches nothing, so a client installed while the program
runs is found the next time the dialog asks. Only folders that exist as directories are
returned, each once, sorted by provider and then by path. Every source is optional: a missing
or unreadable configuration file of one client yields no folder for it and never an error,
because not having a cloud client is the normal case.

## dropbox

Dropbox writes `info.json` with one object per account type, `personal` and `business`, each
with the `path` of its folder. The file lies in `~/.dropbox` on Linux and macOS and in
`%APPDATA%\Dropbox` or `%LOCALAPPDATA%\Dropbox` on Windows, depending on the client's age.
This is the lookup Dropbox documents for other programs.

## nextcloud

The Nextcloud client keeps an ini file `nextcloud.cfg`; every synchronised folder has a key
that ends in `\localPath` inside a group of its account whose name starts with `Folders`
(`Folders` or `FoldersWithPlaceholders`). The value may be quoted and usually ends with a
slash, which the resolution (#resolved-paths) removes. Qt writes characters outside Latin-1 as
`\x` escapes; those are not decoded, so such a folder fails the existence check and is left
out instead of being offered under a wrong name.

## onedrive

The OneDrive client sets `OneDriveConsumer` for a private account, `OneDriveCommercial` for a
work account and `OneDrive` for the one it started last. All three are read and the duplicate
drops out in detect().

## google-drive-windows

Google Drive for desktop streams the files through a virtual drive of its own, which Windows
reports as a fixed drive with the volume label "Google Drive". Only fixed drives are asked for
their label: the question wakes the server behind a network drive
(MountedDrives.md#requested-drive-only). The drive root is offered, not "My Drive" below it,
because that folder's name follows the account's language. A Google Drive set to mirror the
files into a normal folder is not found this way.

## cloud-storage-macos

Since macOS 12 every provider that uses the File Provider framework has its folder in
`~/Library/CloudStorage`, named `<Product>-<Account>`, for instance `OneDrive-Personal` or
`GoogleDrive-name@example.com`. The part before the first hyphen is the provider's name as
shown to the user; only Google's needs a space put in. iCloud Drive is not kept there but in
`~/Library/Mobile Documents/com~apple~CloudDocs`.

## resolved-paths

Paths are made canonical before they are compared, so a folder reached through a symbolic link
(on macOS `~/Dropbox` points into `~/Library/CloudStorage`) is listed once, and a trailing slash
or a `..` in a client's configuration does not make two entries of one folder.
