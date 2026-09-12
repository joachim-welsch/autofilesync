# SyncItem

Module comment for SyncItem.h / SyncItem.cpp.

## sync_empty_files_

Without this flag empty files are skipped: a file of size 0 is usually an aborted write, and
copying it over an existing target would lose data.

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
