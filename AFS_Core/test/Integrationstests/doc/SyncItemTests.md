# SyncItemTests

Module comment for SyncItemTests.cpp.

## exclusions-wildcard-syntax

Exclusions feature: is_excluded() reuses the same */? wildcard syntax the existing wildcard
field already uses (via util::wildmatch()), matched against the entry's own name (not a full
path) - consistent with how the wildcard field itself is applied throughout SyncDir/SyncTree.

## SyncItemStoresAndCopiesExclusions

SyncItem persists/copies its exclusion list like any other field.

## SyncItemTypeKeyStaysUntranslated

sync_item_type_to_str() is translated (GNU gettext), sync_item_type_key() is not: the
configuration file must stay readable no matter which language wrote it.
