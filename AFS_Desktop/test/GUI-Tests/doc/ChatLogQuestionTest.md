# ChatLogQuestionTest

Module comment for ChatLogQuestionTest.cpp.

## write_sample_log_entries

Writes realistic entries into the sandbox's own log file, repeated until the file is at least as
large as ChatDialog's excerpt cap (MAX_LOG_EXCERPT_CHARS in ChatDialog.cpp). Two bugs only showed
up right at that boundary, not with a handful of lines: an empty log short-circuits
LlamaClient::analyze_log() before it ever builds a prompt, and a short excerpt tokenizes to well
under LlamaClient's n_batch limit either way - only a real, near-the-cap excerpt exercises the
path that used to crash the whole process (GGML_ASSERT(n_tokens_all <= n_batch)) and then, once
that was guarded against by raising n_batch instead of capping the prompt, hung for over an hour
in a single oversized decode() call instead.

## ScopedSampleLogEntries

Restores the log file exactly as it was found, even if a fatal ASSERT_* exits the test early -
GUI tests must leave every piece of state the way they found it, and the log is not part of the
ApplicationState GuiTest::TearDown() already checks. A real crash of the process obviously skips
this too, same as it skips every other destructor.

## LogFrageWirdImChatBeantwortet

Regression test for the crash the removed "Log analysieren" button used to cause: a log question
is now asked as an ordinary chat message (see ChatDialog::on_send()), so this drives the dialog
exactly like a user would and waits for the assistant's answer instead of assuming a fixed
delay.

## dialog-is-a-separate-window

ChatDialog is a separate top-level window, not a child of main_window(), so its controls need the
global search (no parent) rather than UserActions::find(). A single pump() is not enough to rely
on: the window manager still has to map and position the dialog before a simulated click at its
screen position lands on the right spot.

## wait-for-the-answer

The assistant answers on a worker thread; waiting for the button to re-enable avoids guessing
how long a real model needs to reply. Capped at 10 minutes: the 7B model runs on the CPU only
(see LlamaClient::ensure_loaded()) and needs about four minutes for the log question on an
Apple M4 - the previous cap of four minutes failed right at that edge - so the cap fires only
on a genuine hang (regression test for exactly that - see LlamaClient::ensure_loaded()/send()
for the two bugs this covered: the batch-size crash, then the batch-size hang).

## hidden-not-closed

Hidden, not closed, mirroring how the dialog's own close handler leaves it - the conversation
stays alive for a later test that opens the chat. Hidden from a destructor, so a test that
gives up on the answer leaves no chat window on screen for the tests after it: an answer that
arrives late would otherwise land in a visible dialog and pull the keyboard focus out of
whatever dialog the next test is typing into.
