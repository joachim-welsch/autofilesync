# CopyLogTests

Module comment for CopyLogTests.cpp.

## read_proof

The whole feature is about what ends up in the file, so every test reads the file back rather
than trusting the writer's return value.

## named-root

Kept in a named variable: get_child() hands back a reference into the tree, and a temporary tree
would already be gone inside the loop.

## entries_of

The list of entries is the only member of the root. It is taken by position, not by name: the
name is translated and differs between the languages.

## member

Fields are looked up by their translated name, for the same reason as capped-list-marker, and
with find() rather than get(): get() takes its argument for a path and would stumble over a
translation that contains a '.'.

## NewProofFileNamesItsEntriesInTheCurrentLanguage

Compared through translation::translate() for the same reason as capped-list-marker.

## ProofFileBegunInAnotherLanguageIsContinuedNotSplit

After a change of language the file still carries the name of the language it was begun in. The
new entry has to join that list: a second list beside it would leave a reader of either one with
half the proof. The dots in the name make sure it is never taken for a path into the tree.

## PassWithoutActivityWritesNoEntry

A pass that found nothing to do proves nothing. With a sync cycle down to one second, writing an
entry for it anyway would bury the passes that did something under thousands that did not.

## SyncFailureEndsUpInTheProof

An unreadable source is a failure the proof has to state, not one it may leave out - otherwise
the file only ever documents what went well. This holds for the default; an item that waives
the warning (SyncItem.md#warn_if_unreachable_) leaves no entry, see Synchronize.md#unreachable_.

## UnreachableSourceIsProvenOnceNotOncePerPass

A source that stays gone is reported when it goes, not on every pass that finds it still gone:
with a cycle down to a second, the second reading would leave a proof file of thousands of
identical entries per day.

## name-survives-sanitising

The unusable characters are gone from the file name, but the entry still states which sync item
it belongs to.

## UnreadableProofFileIsNotOverwritten

The proof file is evidence: an entry that cannot be added must not cost the entries already
there.

## capped-list-marker

The last entry says the list is incomplete, so a capped list is never mistaken for the full
account of what went wrong. Compared through translation::translate(), not against a literal:
the marker is a translated message, and pinning the test to one language would make it fail in
every other one.

## UmlautsInNameAndPathsReachTheProofAsUtf8

Regression test: the proof was written through a std::wofstream in the "C" locale, which failed
on any character up to U+00FF - an umlaut in the item name or a path meant no proof at all. The
second write checks the way back: an existing file with umlauts must be read to be continued.

## ProofIsWrittenWhenFieldNamesAreNotAscii

The same fault hit every proof in French and Polish, whose field names carry such characters,
while Ukrainian only ever went through because write_json() escapes everything above U+00FF.
The catalogues are switched with set_language(), as the settings do.
The status must also stand in the file as plain UTF-8, readable in a text editor, not as \uXXXX
escapes.
