# SyncProposal

Module comment for SyncProposal.h / SyncProposal.cpp.

## sync_proposal

The assistant never writes the configuration itself. It may propose a new synchronisation, the
deletion of an existing one, or one of a few commands in a machine-readable block at the end of
its answer; the chat
shows the proposal as a button, and the user checks and confirms it the ordinary way: a new
item in the SyncItemDialog, which validates it like any hand-entered item, a deletion in the
same confirmation question the delete command asks. Robustness first: a 3B model confuses paths
and names often enough that a silent write would be a trap for exactly the inexperienced users
the chat is for.

## Deletion

The second kind of proposal: a block {"delete": "name"} with the name of a configured item, as
the instruction asks for it. A block is a deletion when it carries a non-empty "delete" field,
otherwise it is read as a new item; so a model that writes both fields proposes a deletion,
which is the safer reading because the user has to confirm it anyway.

## instruction

The part of the system prompt that defines the block. Assembled at call time because it names
the item types in the catalogue language - the model answers in that language and writes the
type the way it read it, and str_to_sync_item_type() takes translated names as well as the
keys.

## parse

The first fenced block whose body is JSON with the required fields of a new item or a deletion,
or a bare JSON object if the model forgot the fence. Empty if the answer carries no usable
proposal; a chat answer without one is the normal case, not an error.

## fenced-or-bare

Models of this size follow the fenced form most of the time but not always: an unfenced object
somewhere in the prose is still a proposal the user can check, so it is accepted as a fallback.

## required-fields

name, source and target must all be present and non-empty: the instruction tells the model to
ask for a missing path instead of inventing one, and a proposal with a placeholder path would
only be rejected by the dialog's validation anyway. A missing or unknown type becomes a
directory tree, the most common choice; the user sees and can change it in the dialog.

## without_block

The answer as shown to the user: the block is replaced by the button, so the JSON would only
confuse. The full answer still goes into the history so the model sees what it proposed.

## names_two_paths

Whether a question can carry a complete proposal at all: a new synchronisation needs a source
and a target, and the user names them as paths. A word containing a slash or backslash counts
as a path, on Linux, on Windows and in the SSH form "account:/path". Two such words are the
trigger for the extraction pass (see LlamaClient.md#with_proposal); questions without paths
never pay for it.

## names_configured_item

The second trigger for the extraction pass: a deletion names no path, but it names an existing
synchronisation. The comparison ignores case because users type names from memory.

## extraction

The question the client appends after a free answer that carried no block although the user
named two paths. A 3B model does not always follow the instruction to append a block when it
also writes an explanation; asked only for the object, with the grammar enforcing the form, it
is far more reliable. "null" is the way out when the user did not want a new synchronisation,
so the model is not pushed into inventing one.

## grammar

GBNF for the extraction pass: the object with exactly the four fields, a deletion, or the word
null. The type is restricted to the three names the dialog shows, so the model cannot produce a
type parse() would have to guess at. Strings exclude unescaped quotes, backslashes and line
breaks, which keeps a model that starts rambling from producing an unterminated string.

## configured-names

The deletion rule allows only the names of configured items, so the extraction cannot propose
to delete something that does not exist or misspell a name; without configured items the rule
is left out. Names are escaped for GBNF by gbnf_literal.

## gbnf_literal

A GBNF literal that matches the JSON string of a text: a quote or backslash in the text is
escaped once for JSON, so the model emits valid JSON, and the result escaped again for GBNF;
every other character including non-ASCII is taken as it is.

## as_block

Wraps the extracted object the way the instruction asks for it in a free answer, so that the
answer in the history, parse() and without_block() see one and the same form.

## describe

One sentence naming the proposed item or deletion and the button that applies it. Small models
often answer with the block alone; with the block removed the user would see an empty answer
and could easily miss the button, which happened in practice. The sentence is shown with every
proposal, so the user also sees at a glance what the dialog will contain. The type is named the
way the dialog shows it, so it reads the same in both places.

## Command

Commands the user could run from the menu anyway: synchronise once, start or stop the
synchronisation, start or stop the background service, show the log file. None of them changes
the configuration or deletes anything that a sync would not, so a button that runs them adds no
risk. The JSON keys are fixed and untranslated; the model learns them, with their names in the
current language, from command_list().

## label

The menu item's own text, without the mnemonic "&" and a trailing ellipsis, so the button names
the command the way the user knows it from the menu.

## command_list

"sync_once (Einmal synchronisieren), ..." - the key the block must use next to the name the
user and the model read, for the instruction and the extraction question alike.

## names_command

Triggers the grammar-bound extraction pass for a question that asks for a command. The words
come from the catalogue, imperative stems such as "synchronisier", "starte", "öffne", since a
3B model often answers such a request in prose and forgets the block. A false trigger costs one
short pass that usually yields null.
