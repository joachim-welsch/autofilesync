# LlamaClient

Module comment for LlamaClient.h / LlamaClient.cpp.

## LlamaClient

Runs a GGUF model locally through llama.cpp - no network, no API key. Which file is decided by
model_location::find() (see ModelLocation.md); nothing is loaded until the first send(), so a
missing model costs nothing at startup.

## available

True if a model file was found; does not load the model, so this is cheap enough to call whenever
the chat window opens.

## missing_model_hint

Names both ways to provide a model - the directory and the variable - because the user reading
it has neither, and a hint naming only the variable is useless to someone who never opened an
environment settings dialog.

## utf8-path

llama.cpp takes the file name as UTF-8 and widens it itself on Windows; the narrow path string
would be in the ANSI code page there and break on non-ASCII user names.

## send

Blocks until the answer arrives or generation fails; never throws, so a worker thread cannot
terminate the program. Loads the model on the first call and keeps it for the lifetime of this
object; later calls only pay for re-decoding the (short) chat history.

## analyze_log

Answers the latest turn of history with a log excerpt (see read_recent_log_entries()) folded
into it. Only this one request sees the excerpt - the caller's own copy of history is untouched,
so a persisted ChatHistory never carries it forward into every later send() call.

## build_prompt

Renders the history through the model's own chat template; empty if the model carries no
template llama.cpp recognises.

## mutex_

Serialises send(): a llama_context's decode/KV-cache state is not safe to touch from two threads
at once, and ChatDialog disables the button while a reply is pending but does not guarantee only
one caller exists.

## SYSTEM_PROMPT

Sent as the "system" turn on every request: the model has no other way to know what Autofilesync
is, since there is no RAG/document lookup. Kept as a message id and translated in build_prompt()
instead of at namespace scope - it also asks for the answer's language, and the catalogue is not
bound yet while the constants are initialised.

## LOG_ANALYSIS_INSTRUCTION

Sent in front of the excerpt for analyze_log(): a plain log dump without an instruction leaves
the model guessing what kind of answer is wanted. A message id like SYSTEM_PROMPT; analyze_log()
adds the blank lines that separate it from the excerpt, so no translation can drop them.

## backend-init

Idempotent and required exactly once before any other llama_* call; cheapest to do lazily here
rather than force every caller to remember it.

## batch-size

n_batch is deliberately left at its library default (2048), not raised to match n_ctx: send()
hands the whole prompt to llama_decode() in one llama_batch_get_one() call, and a
several-thousand-token prompt (e.g. a log excerpt) submitted as a single batch this large
measured drastically slower on the CPU backend than the same tokens processed within the default
batch size - minutes instead of seconds. send() checks the prompt against llama_n_batch() before
ever calling decode, so a prompt this large is refused outright, not hung.

## contents-lifetime

llama_chat_message.content points into these, so they must outlive the
llama_chat_apply_template() calls below.

## kv-cache-cleared

Every turn re-sends the whole history (see ChatTypes.h), so the previous turn's KV cache is
stale weight, not something to build on.

## prompt-size-check

Checked against n_batch, not just n_ctx: send() submits the whole prompt to llama_decode() as a
single batch (see ensure_loaded()), so a prompt that fits the context window can still exceed
what one batch may carry - GGML_ASSERT(n_tokens_all <= n_batch) aborts the whole process instead
of just this request, and there is no recovering from that here.

## token-budget

Thinking and answer share this budget - a fixed cap keeps one runaway reply from filling the
whole context window.
