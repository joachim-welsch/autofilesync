# CopyLogTests

Module comment for CopyLogTests.cpp.

## read_proof

The whole feature is about what ends up in the file, so every test reads the file back rather
than trusting the writer's return value.

## named-root

Kept in a named variable: get_child() hands back a reference into the tree, and a temporary tree
would already be gone inside the loop.

## PassWithoutActivityWritesNoEntry

A pass that found nothing to do proves nothing. With a sync cycle down to one second, writing an
entry for it anyway would bury the passes that did something under thousands that did not.

## SyncFailureEndsUpInTheProof

An unreadable source is a failure the proof has to state, not one it may leave out - otherwise
the file only ever documents what went well.

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
account of what went wrong.
