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

Blocks until the answer is complete or generation fails; never throws, so a worker thread cannot
terminate the program. Loads the model on the first call and keeps it for the lifetime of this
object; later calls only decode what changed since the last one (see kv-cache-reuse). on_piece,
if set, receives the answer while it is generated, on the calling thread.

## ChatPieceSink

Receives consecutive pieces of the answer; concatenated they give the answer send() returns.
On a CPU an answer takes tens of seconds, and the user reading along while it grows waits only
for the first words instead of for the last.

## complete_utf8_length

A token can end in the middle of a multi-byte UTF-8 character (umlauts, emoji); the incomplete
tail is held back until the next token completes it, since converting it alone would garble
the character.

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

n_batch is deliberately left at its library default (2048), not raised to match n_ctx: a
several-thousand-token prompt (e.g. a log excerpt) submitted as a single batch this large
measured drastically slower on the CPU backend than the same tokens processed within the default
batch size - minutes instead of seconds. decode_prompt() therefore hands the prompt to
llama_decode() in chunks of at most n_batch tokens; one batch larger than n_batch would abort
the whole process with GGML_ASSERT(n_tokens_all <= n_batch).

## contents-lifetime

llama_chat_message.content points into these, so they must outlive the
llama_chat_apply_template() calls below.

## cached_tokens_

The tokens whose keys and values are in the KV cache right now: the last prompt plus every
answer token decoded after it. Cleared together with the cache whenever a decode fails, since
the cache content is unknown then.

## decode_prompt

Decodes only the part of the prompt that differs from cached_tokens_. Every turn re-sends the
whole history (see ChatTypes.h), and the history only grows at its end, so usually only the
new question has to be processed - instead of the whole conversation, which on a CPU took
seconds for every earlier turn.

## kv-cache-reuse

Removes everything behind the common prefix, i.e. the part where the new prompt differs: the
template's end-of-turn tokens after the last answer, which were never decoded, or a log excerpt
analyze_log() folded into an earlier question only. Some memory types cannot remove a partial
range; then the cache is cleared and the whole prompt decoded, as without reuse.

## last-token-decoded

At least the last prompt token is always decoded again: sampling needs its logits, and those
are not kept in the cache.

## generate

Samples until an end-of-generation token or the token budget; every token but the final
end-of-generation one is decoded, so it ends up in the cache for the next turn.

## prompt-size-check

Only the context window limits the prompt: decode_prompt() splits it into batches (see
batch-size). A prompt that fills the window leaves no room for the answer either.

## token-budget

Thinking and answer share this budget - a fixed cap keeps one runaway reply from filling the
whole context window.
