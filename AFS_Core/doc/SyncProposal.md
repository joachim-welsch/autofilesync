# SyncProposal

Module comment for SyncProposal.h / SyncProposal.cpp.

## sync_proposal

The assistant never writes the configuration itself. It may propose a synchronisation in a
machine-readable block at the end of its answer; the chat shows the proposal as a button, and
the user checks and confirms it in the ordinary SyncItemDialog, which validates it like any
hand-entered item. Robustness first: a 3B model confuses paths and names often enough that a
silent write would be a trap for exactly the inexperienced users the chat is for.

## instruction

The part of the system prompt that defines the block. Assembled at call time because it names
the item types in the catalogue language - the model answers in that language and writes the
type the way it read it, and str_to_sync_item_type() takes translated names as well as the keys.

## parse

The first fenced block whose body is JSON with the required fields, or a bare JSON object if
the model forgot the fence. Empty if the answer carries no usable proposal; a chat answer
without one is the normal case, not an error.

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

## extraction

The question the client appends after a free answer that carried no block although the user
named two paths. A 3B model does not always follow the instruction to append a block when it
also writes an explanation; asked only for the object, with the grammar enforcing the form, it
is far more reliable. "null" is the way out when the user did not want a new synchronisation,
so the model is not pushed into inventing one.

## grammar

GBNF for the extraction pass: either the object with exactly the four fields or the word null.
The type is restricted to the three names the dialog shows, so the model cannot produce a type
parse() would have to guess at. Strings exclude unescaped quotes, backslashes and line breaks,
which keeps a model that starts rambling from producing an unterminated string.

## as_block

Wraps the extracted object the way the instruction asks for it in a free answer, so that the
answer in the history, parse() and without_block() see one and the same form.

## describe

One sentence naming the proposed item and the button that opens it. Small models often answer
with the block alone; with the block removed the user would see an empty answer and could
easily miss the button, which happened in practice. The sentence is shown with every proposal,
so the user also sees at a glance what the dialog will contain. The type is named the way the
dialog shows it, so it reads the same in both places.
