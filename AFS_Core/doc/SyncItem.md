# SyncItem

Module comment for SyncItem.h / SyncItem.cpp.

## sync_empty_files_

Without this flag empty files are skipped: a file of size 0 is usually an aborted write, and
copying it over an existing target would lose data.

## warn_if_unreachable_

A source or target that is not there is not an error: a backup disk is plugged in now and
then, a laptop is not always on its home network. Whether the user wants to hear about it is
their call per synchronisation. On, the default, a pass that finds the item unreachable logs
a warning and the GUI shows it after a single run (SyncInterface.md#unreachable-warnings);
off, the pass notes it at info level and moves on. Either way the item is skipped and
nothing is deleted.

## time_tolerance_seconds_

How far apart two write times may be and still count as the same, see
FileDifference.md#time-tolerance. It lives with the comparison criteria because it qualifies
the time criterion and travels with them into FileDifference. DEFAULT_TIME_TOLERANCE_SECONDS
is the value an item gets when the configuration file predates the setting; two seconds is
the coarsest timestamp a common file system (FAT32) stores.

## clone

Deep copy for SyncItem's value semantics; the concrete type stays intact.

## is_excluded

True if `name` (a filename or directory name, not a full path - matching how the existing
wildcard field is applied) matches any exclusion pattern. Patterns use the same */? wildcard
syntax as the wildcard field itself.

## rename_ssh_account_reference

Updates an embedded SSH account reference in-place (path unchanged) when it matches oldName -
see Config::update_ssh_account_references() for why.

## SshPath-strips-account-prefix

Stores the path without its account prefix ("laptop:/home/x" -> "/home/x"); path() puts the
prefix back.

## SYNC_ITEM_TYPE_NAMES

The key and the message id are separate columns because they answer to different masters: the
key is the German word the configuration has stored since the first version and must never
change again, while the message id only addresses a catalogue entry and may be renamed with the
rest of them. Tying the stored value to the message id once already meant that renaming an id
would silently turn every configured item into SyncItemType::Undefined.

## sync_item_type_from_str

Three spellings have to be understood: the key a configuration file stores, the message id
itself (what an untranslated build displays), and the translated text a dialog hands back.

## ssh-path-without-round-trip

SshPath strips the account prefix itself; the detour over the narrow get_remote_path() only
re-widened the UTF-8 bytes one by one and turned every non-ASCII character into an unencodable
wchar_t.

## rename-account-references

source_/target_ hold a *copy* of the SshAccount taken when the path was set (see
source()/target() above), not a live lookup - so renaming or editing the account in Config
leaves existing items pointing at a name that no longer resolves. Config calls this on every
item after a successful account edit to keep them in sync.

## LastRun

The outcome of the item's last pass in this program: whether source and target were reachable,
how many errors there were and the first of them. Kept on the item like last_sync_, not
written to the configuration, so that the assistant can say why an item is not up to date.

## known_files

How many files the item's sync state knew after its last pass (Synchronize.md#sync-state);
empty for an item without a state.

## track_moves_

Whether a one-way item keeps a sync state so that a file renamed or moved in the source is
renamed in the target instead of copied afresh and deleted (SyncFile.md#renames). A two-way
item has the state anyway and tracks moves always; the flag is for the one-way item that
wants the saving, at the price of the state file. Off by default, like every option that
adds a file next to the configuration.
