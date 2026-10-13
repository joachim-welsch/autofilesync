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

## MAX_ITEMS

Bounds the prompt: the context window is 8192 tokens and an item line is 40 to 60 tokens, so
30 items leave room for the system prompt, a log excerpt and the answer. The remaining items
are only counted.

## no-passwords

An account is named with user and host, never with its password: the prompt goes to a local
model only, but it is also what a log of the chat or a bug report would reproduce.
