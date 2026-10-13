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
