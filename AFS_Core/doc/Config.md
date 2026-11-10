# Config

Module comment for Config.h / Config.cpp.

## syncitems_mutex_

Guards syncitems_: the GUI thread (add/remove/edit sync items) mutates it while the sync worker
thread reads a snapshot of it concurrently.

## ssh_accounts_mutex_

Guards ssh_accounts_: the GUI thread (SshAccountsDialog) mutates it while the sync worker thread
searches it concurrently.

## find

find/find_by_name return copies, not pointers into syncitems_: a raw pointer would be written by the GUI thread without the lock (and could dangle after add/remove reallocates) while the sync thread snapshots the vector. Mutations go through update_by_name()/erase_by_name() instead.

## syncitems

Returns a snapshot copy, not a reference: the sync worker thread reads this concurrently with GUI-thread add()/set()/remove()/erase() calls, and SyncDir/ SyncFile/SyncTree hold their SyncItem for the duration of a (possibly slow) copy, so a live reference into syncitems_ could be reallocated/erased from under them.

## ssh-account-access

No direct ssh_accounts_ reference is exposed: callers on different threads must not iterate/mutate the shared vector without going through the lock.

## update_ssh_account_references

Keeps sync items pointing at a renamed/edited SSH account working: their embedded account snapshot (see SyncItem::rename_ssh_account_reference) is updated in place wherever it still matches oldName. Call after a successful update_ssh_account() whose old name you captured beforehand.

## migrate_legacy_config

One-time move so users keep their settings after the config directory changed.

## exclusions-node-format

Mirrors write_sync_item()'s exclusionsNode: a JSON array of plain strings, each pushed with an
empty key (i.first) and the pattern itself as the node's own data (i.second.data()).

## defaulted-flag-reads

Default instead of the throwing get<>(): config files written before this flag existed must keep
loading, and off is the documented default - except warnIfUnreachable, whose default is on
(SyncItem.md#warn_if_unreachable_). The time tolerance reads the same way, with
DEFAULT_TIME_TOLERANCE_SECONDS, so that an existing backup to NTFS or FAT stops copying
everything on every pass with the update that introduced it.

## delete-mode-default

Default instead of the throwing get<>() above: a config file written before the delete mode
existed must keep loading, and the safer recycle bin mode is what it then falls back to.

## read_stored_password

Same rules as read_ssh_accounts(): a value without the encryption marker is genuine legacy
plaintext, while a failed decryption must not be handed on - the ciphertext blob would end up
being sent as the password.

## logging-notification-defaults

Defaulted reads: the notification keys did not exist in older configurations, and the throwing
get<> above would have taken the whole Logging section down with them.

## recycle-bin-path-default

Read with a default, unlike the fields in get_settings(): a config file written before the
recycle bin existed has no such key, and the throwing get<>() would discard every other setting
with it.

## per-account-error-handling

Caught per account, not around the whole loop: one malformed entry must not discard every other
account - see interpret_sync_item()'s per-item handling below for the same reasoning.

## no-ciphertext-fallback

Do NOT fall back to storedPassword here: it is the raw "$AES256GCM$..." ciphertext blob, not a
usable password - using it as one would submit garbage to the SSH server with no indication of
what actually went wrong (lost/changed key).

## smtp-password-encryption

Encrypted like the SSH passwords; an empty password stays empty so that read_stored_password()
does not have to tell "unset" from "undecryptable". Where the stored one could not be decrypted,
its ciphertext is written back unchanged (SmtpMailer.md#sealed_password).

## untranslated-type-key

The untranslated key, not the displayed text: a configuration written with one language must
stay readable with any other.

## disk_time_

The modification time of the configuration file as read() found it or write() left it. The
daemon has no other channel to the GUI than this file, so file_changed_on_disk() comparing the
current time against it is how it learns that the user saved. read() records the time before
parsing, and whether or not parsing succeeds: a save that lands between the two is reported as a
change on the next poll, and a defective file is not re-read once a second.

## file_changed_on_disk

True once the file at pathname() carries another modification time than the last read() or
write() recorded. A GUI save goes through a temporary file and a rename, so a poll never sees a
half-written file.

## utf8-file

The configuration file is UTF-8, written and read through util::write_utf8_file() and
util::read_utf8_file(); a wide file stream could not save a name or path with an umlaut on Linux
and macOS - see Util.md#utf8-files. json::to_text() writes every character as itself, so the
file stays readable in a text editor whatever the language of its names and paths.

## record_last_run

Like refresh_last_sync(): the pass works on a snapshot of the items, so the outcome must go
into the live item under the lock, where the next snapshot and the assistant find it.

## state-leaves-with-item

The sync state file belongs to the item it is named after; a deleted item would leave it
behind for good, and an item created later under the same name would start with a foreign
state - which read_sync_state() rejects only when the paths differ.

## devices

The phones paired with this configuration (Device.md), under "Devices" in the file, kept and
guarded like the SSH accounts: DeviceLink's thread registers a device and looks one up while the
GUI thread lists or unpairs them. Devices are keyed by their id, not by position, because the
phone names itself by id in every message. Adding a device marks the configuration changed, so
the GUI's autosave persists a pairing; the daemon, once it listens as well, must write the file
itself.

## ssh-port

Read with a default of 22, like the other keys that did not exist in older configurations.
Written always, so a file saved by this version names the port it uses.

## device-accounts

A phone is an SSH account whose name is the device id (Device.md#Device): the sync items
refer to it as "<id>:/path", exactly like an account from the SSH connections list, and every
SSH operation finds it through find_ssh_account_by_name(). The account's host, port, user and
password change with every contact, so they are not persisted: DeviceLink sets them for the
pass the phone is serving (Device.md#serving) and clears them afterwards. The lookup tries the
configured accounts first, then the live device accounts, then a placeholder.

## device-placeholder

A device that is paired but not serving still needs an account, or SshPath would lose the
prefix of the path (SyncItem.md#SshPath-strips-account-prefix) and CheckPath would report a
missing account as an error. The placeholder has the id as name and the stored host key, but
no host: every connection to it fails as unreachable, which is the truth while the phone is
away, and Synchronize skips such items anyway (Synchronize.md#device-items).
