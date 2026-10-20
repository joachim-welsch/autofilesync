# ConfigSummary

Module comment for ConfigSummary.h / ConfigSummary.cpp.

## config_summary

Renders the configuration as text for the assistant: a language model knows nothing but the
prompt, so a question like "how many synchronisations are configured" can only be answered
from a description that is in the prompt. Kept apart from LlamaClient so the text can be
tested without loading a model.

## describe

Takes the items and accounts instead of reading Config itself so tests can describe any
configuration. The count stands in its own line before the list: a small model answers the
counting question from that line instead of counting list entries, which it does badly. Lines
are built in the catalogue language, the same one the system prompt asks the answer in.

## current

The live configuration; syncitems() and ssh_accounts_snapshot() copy under Config's locks, so
this is safe from the chat's worker thread.

## describe_item

One line per item, deterministic in field order and wording: the text is part of the prompt
prefix the KV cache reuses (see LlamaClient.md#config-in-system-prompt), so the same
configuration must always render to the same text. Only options that are switched on are
listed; a list of everything that is off would double the token count for nothing.

## unreachable-explained

"Why did I get a warning here and none there?" is a question the model can only answer if
the line carries both halves: the option (SyncItem.md#warn_if_unreachable_) and what the
last pass found. The option is listed when it is off, the one deviation from the
only-on rule above, because off is the non-default and the state that explains a missing
warning. The unreachable line of the last run then spells the consequence out in full -
skipped with a warning because the option is on, or skipped silently because it is off - so
the model does not have to combine two facts to explain one box; the system prompt states
the rule in general terms (po: assistant_system_prompt).

## MAX_ITEMS

Bounds the prompt: the context window is 8192 tokens and an item line is 40 to 60 tokens, so
30 items leave room for the system prompt, a log excerpt and the answer. The remaining items
are only counted.

## no-passwords

An account is named with user and host, never with its password: the prompt goes to a local
model only, but it is also what a log of the chat or a bug report would reproduce.

## last_sync_text

Minutes since the last pass, from the steady clock that drives the sync cycle; it has no date,
and "how long ago" is what a question about a stale target needs. "Not yet" means not in this
program: a background service syncs in a process of its own.

## MAX_ERROR_CHARS

The first error of a run often carries a long path. Two hundred characters keep its meaning
and thirty items within the context window.

## known-files

A two-way item that has run in this program says how many files its sync state knows, so the
assistant can explain that a deletion on one side will be carried to the other, and that a
first pass after an update cannot yet (SyncFile.md#state).
