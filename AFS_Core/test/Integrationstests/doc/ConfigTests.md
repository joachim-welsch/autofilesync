# ConfigTests

Module comment for ConfigTests.cpp.

## REDIRECTED_VARIABLES

The variables Config::get_root_pathname() derives the configuration path from - APPDATA plus
USERPROFILE for the legacy location on Windows, HOME (and XDG_CONFIG_HOME on Linux) elsewhere.

## ConfigPathTest

Points the environment at a fresh temp directory so Config::get_root_pathname() can be exercised
without touching the real user configuration.

## write_legacy_plaintext_config

Writes a config file in the pre-encryption format (plaintext "password" field) to verify
existing users' configs still load after upgrading.

## xdg-linux-only

XDG_CONFIG_HOME is a Linux/freedesktop.org convention only: on macOS Config::get_root_pathname()
always resolves under Library/Application Support, on Windows under APPDATA, so these two apply
to Linux alone.

## utf8-password-roundtrip

Passwords are arbitrary UTF-8 byte sequences; every stage of the chain (AES-GCM, base64, JSON
with its wide-string node values) has to hand back exactly the same bytes, including the ones a
JSON writer must escape.

## ConcurrentSshAccountAccessDoesNotCrash

Regression test for Fehler 9: sshAccounts() used to hand out a live reference to the internal
vector, so a GUI-thread erase()/edit could reallocate or mutate elements while a sync-thread
search was iterating - classic concurrent-modification UB. Config now only exposes locked,
copying accessors; hammering add/remove/find from multiple threads at once must not crash and
must always see a consistent (never partially-erased) state.

## ConcurrentSyncItemAccessDoesNotCrash

syncitems_ is read by the sync worker thread (via the snapshot returned from syncitems()) while
the GUI thread concurrently adds/removes items - the same hazard already covered above for
ssh_accounts_.

## RefreshLastSyncUpdatesTheLiveItem

refresh_last_sync() is how Synchronize writes the result of syncing a snapshot copy back onto
the live SyncItem in Config (see Fehler 2: the worker no longer holds a reference into
syncitems_ directly). Without this write-back, last_sync() would stay at its initial value
forever and per-item sync_cycle throttling (pause_elapsed() in Synchronize.cpp) would silently
break.

## WriteFailureDoesNotCorruptExistingConfig

Fehler 5: write() used to open pathname directly with std::ios_base::out, which truncates it
immediately - a write_json failure partway through left a half-written/corrupt file with the
previous, working config gone. write() now writes to a "<pathname>.tmp" file and renames it over
pathname, so a failure must leave the original file completely untouched.

## occupied-temp-path

Make the temp file write() would create fail by pre-occupying its path with a directory -
opening it as an output file fails.

## FindByNameReturnsCopyAndUpdateByNameWritesBack

Fehler 7 (round 4): find()/find_by_name() used to hand out raw SyncItem* into the mutex-guarded
vector; the GUI then wrote through that pointer without the lock (and it dangled after
add/remove reallocated the vector) while the sync thread concurrently copied the vector. They
now return copies, and writes go through update_by_name() under the lock.

## MalformedSyncItemIsSkippedNotHalfLoaded

Fehler 9 (round 4): interpret_json() ignored interpret_sync_item()'s return value and added the
half-initialized SyncItem anyway. One malformed entry (e.g. a missing mandatory field) produced
a ghost item - enabled by default, type Undefined, empty paths - that showed up in the GUI and
ran in every sync pass. Malformed entries must be skipped while the valid rest loads.

## broken-first-entry

First entry lacks "name" (and more), second is complete - the loop must survive the broken one
and still load the good one.

## RenamingSshAccountUpdatesReferencingSyncItems

Fehler 5 (round 5): a SyncItem's embedded SSH account is a snapshot taken when its source/target
path was set, not a live lookup by name (see SyncItem::source()/target() via
get_account_from_path()). Renaming or editing the account in SshAccountsDialog left existing
items pointing at a name/ credentials that no longer resolve in Config, so they silently failed
every sync pass afterward with no indication why.

## rename-without-references

Renaming an account nothing references is a no-op, not an error.

## MalformedSshAccountIsSkippedNotAllAccountsLost

Fehler 3 (round 6): a single malformed SSH account entry aborted the whole read_ssh_accounts()
loop, and a single malformed Settings/Logging field aborted interpret_json() before
SshAccounts/Syncitems were even read - one bad field anywhere near the top of the file could
make every sync item and every other SSH account appear to vanish. Both sections are now
resilient: per-account for SshAccounts (mirroring the existing per-item Syncitems handling), and
non-fatal for Settings/Logging.

## missing-settings-fields

"Settings" node present but missing every field it needs; "Logging" absent entirely (also
handled via get_child_optional, but exercises the same non-fatal-failure path for Settings
specifically here).

## FailedPasswordDecryptionDoesNotUseRawCiphertextAsPassword

Fehler 4 (round 7): read_ssh_accounts() used decrypted.value_or(storedPassword), conflating
"genuine legacy plaintext" (decrypt() returns nullopt because there's no marker) with "encrypted
but failed to decrypt" (decrypt() also returns nullopt, e.g. the encryption key was
lost/changed) - the second case silently used the raw "$AES256GCM$..." ciphertext blob as the
literal SSH password instead of surfacing that decryption failed.

## undecryptable-password

Well-formed marker + base64 blob (long enough to pass the corruption length check) that can
never authenticate against any key - decrypt() must fail, and the account's password must not
become this raw string.

## ExclusionsRoundTripThroughSaveAndLoad

Exclusions feature: get_exclusions() was a TODO stub that always returned empty, so exclusion
patterns written to the config file were silently dropped on the next load - write_sync_item()
already wrote them correctly, but nothing read them back.

## BackupProofDefaultsToFalseForOlderConfigFiles

A config file written before the flag existed has no "backup_proof" node; loading it must fall
back to the default instead of throwing.

## DeleteInTargetRoundTripsThroughSaveAndLoad

"Gelöschte Dateien im Ziel ebenfalls löschen" is persisted under the syncflags key
"syncDeletions" (write_sync_item/get_sync_flags). The option deletes user data, so a silent loss
of the flag on save/load - or a silent gain of it - is the kind of defect that has to be caught
here rather than in the field.

## DeleteInTargetStaysOffThroughSaveAndLoad

The opposite direction matters just as much: an item saved with the option off must never come
back with it on.

## DeleteInTargetDefaultsToFalseForOlderConfigFiles

A config file written before the flag existed has no "syncDeletions" node. Loading it must
default to off - defaulting to on would start deleting target files on the next sync of an item
the user never opted in for.

## SyncEmptyFilesRoundTripsThroughSaveAndLoad

"Leere Dateien synchronisieren" is persisted under the syncflags key "sync_empty_files". The
same applies as for syncDeletions: if the flag is lost on save or load - or gained unasked - the
behaviour towards the user's data changes.

## SyncEmptyFilesDefaultsToFalseForOlderConfigFiles

A configuration file written before the flag existed does not know the key. It has to load with
the flag switched off - otherwise an existing installation would start transferring empty files
unasked after the update, possibly overwriting target files that have content.

## ConfigWithoutDeleteModeFallsBackToRecycleBin

A config file written before the delete mode existed has no DeleteMode key: it must fall back to
the recoverable mode rather than fail the Settings node.

## ConfigWithoutRecycleBinPathKeepsCurrentPath

A config file written before the recycle bin existed has no RecycleBinPath key: it must keep the
configured path instead of blanking it, and must not take the rest of the Settings node down
with it.

## FileChangedOnDiskFollowsReadAndWrite

The daemon relies on file_changed_on_disk() to notice a save by the GUI, so the three
transitions are pinned: the own write() is not a change, a file with a newer modification time
is one (set two seconds ahead rather than rewritten, so the test does not depend on the file
system's time stamp resolution), and read() takes the new time over. The second test covers the
file that does not exist yet: read() of a missing file records the "no file" time, so the file
being created afterwards is a change too.

## UmlautsRoundTripThroughSaveAndLoad

Regression test: the configuration was written through a std::wfstream in the "C" locale, which
failed on the first umlaut on Linux and macOS - write() returned false, and every change of a
user whose paths contain one was lost at the next start. Name, paths, an exclusion and a setting
cover each kind of text the file holds.

## NonLatinNamesStayReadableInTheFile

The configuration is meant to be readable and editable by hand. write_json() escaped every
character above U+00FF and clamped those above U+FFFF, so a Ukrainian path read as \u041E... and
an emoji in a name came back as U+FFFF. The name must stand in the file as UTF-8 and survive the
round trip unchanged.
