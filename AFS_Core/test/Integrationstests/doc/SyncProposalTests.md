# SyncProposalTests

Module comment for SyncProposalTests.cpp.

The blocks use the German type keys ("Verzeichnisbaum", "Verzeichnis"), which
str_to_sync_item_type() accepts in every language, so the tests do not depend on a bound
catalogue.

## InstructionNamesTypesAndFields

Checks the field names, which are the contract between the instruction and parse(), and that
the type list was substituted. That needs a real catalogue, so it binds the German one the way
TranslationTests do and skips where none is built; the locale is restored so later tests see
the bare keys as before.

## TwoPathsInAQuestionTriggerTheExtraction

The trigger must fire for the ways users write paths (Linux, Windows drive, SSH account:/path)
and must stay quiet for a question with one path or none, since every false trigger costs a
second model pass on an ordinary question.

## DeleteBlockIsADeletion

A "delete" block is the second kind of proposal; an empty name is none, and a block with only
a name and no paths is none either, so a model that drops the "delete" key cannot turn a
deletion into a half-filled creation.

## GrammarRestrictsDeletionToConfiguredNames

The deletion alternative lists exactly the configured names as GBNF literals, with quotes and
backslashes in a name escaped; the expected text is the GBNF source, hence the doubled escapes.

## DescriptionNamesItemAndButton

The description is what the user reads when the model answers with the block alone, so it
must carry the name, both paths, the type as the dialog shows it, and the button label, with
every placeholder substituted. Binds the German catalogue like InstructionNamesTypesAndFields.

## MissingPathIsNoProposal

An empty target and a missing target are the two ways a model leaves a path out; both must be
treated as "no proposal", or the user would get a button that opens a dialog with an empty
field - the dialog's validation would catch it, but the button promised a proposal.

## BareJsonWithoutFenceIsAccepted

The fallback for a model that drops the fence. The fields "type" and "two_way" are optional,
so this test also covers that they are taken when present.
