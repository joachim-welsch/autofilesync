# ChatDialog

Module comment for ChatDialog.h / ChatDialog.cpp.

## ChatDialog

Chat with the assistant. Modeless, so a long answer never blocks the rest of the program, and
hidden instead of destroyed on close so the conversation survives until the program ends.

## Pending

Shared with the worker thread. The worker may only reach the dialog while it holds the lock and
dialog_ is still set; the destructor clears it under the same lock, so an answer arriving after
the dialog is gone is dropped instead of being posted to a destroyed window.

## runAsync

Runs call() on a worker thread and posts its ChatReply back to onReply(); the pieces of the
answer that call() passes to its ChatPieceSink go to onPiece() while it runs.

## answerStreaming_

Set once the first piece of an answer is in the transcript: the speaker line is written then,
and onReply() replaces the streamed text with the final answer instead of appending it again.

## CreateSync

How a proposal of the assistant becomes a synchronisation: MainWindow passes its prefilled
"add synchronisation" dialog (MainWindow.md#addSyncItemFrom). A callback rather than a pointer
to MainWindow keeps the dialog free of the main window's interface.

## proposal-button

The button is the proposal: hidden until an answer carries a block parse() accepts, hidden
again when the next question is sent or the button was used. A visible but disabled button
would raise the question what it is for.

## proposal-described

An answer with a proposal always ends with the sentence from SyncProposal.md#describe. The
3B model regularly answers with the block alone; without the sentence the turn would be empty
and the button the only hint that something happened - a user who expects an answer in the
transcript reads that as "the assistant does not answer".

## block-held-back

While the answer streams, nothing from the first code fence on is shown: the block is JSON
meant for parse(), and watching it appear character by character would only puzzle the user.
Text before the fence still appears as it is generated.

## replaceStreamedTurn

The final answer replaces the streamed text from the start of the turn: with the block removed
(see SyncProposal.md#without_block), and with any character the stream held back completed.

## pieces-before-reply

Queued from the same thread as the final reply, so wx delivers every piece before onReply().

## client_

Shared rather than a plain member: the worker thread needs the loaded model to stay alive for
the send() call, and LlamaClient's llama_model/ llama_context members are non-copyable.

## ID_CHAT_TRANSCRIPT

Stable id for the read-only transcript, otherwise wxID_ANY: GUI tests need it to read the
conversation back via FindWindowById().

## maxLogExcerptChars

Bounded by response time, not by n_batch/n_ctx: fitting inside the token budget (see
LlamaClient::ensure_loaded()) isn't enough on its own - CPU prompt processing of a
several-thousand-token excerpt measured minutes, not seconds, on ordinary hardware (see the
LogFrageWirdImChatBeantwortet GUI test, which used to time out at 3000 chars). 500 chars
measured well under a minute for a real answer; kept small deliberately since larger values were
not just slower but disproportionately slower.

## key-handler-on-control

A key handler on the control, not in the event table: the table belongs to the dialog and never
sees the keys the text field consumes.

## deferred-focus

Deferred: the dialog is not shown yet at construction time (MainWindow::onChat creates it, then
calls Show()), and a widget that is not yet realised cannot reliably take the X input focus -
the very first keystroke could go nowhere.

## history-snapshot

The history is copied for the async call: the dialog keeps accepting input while the request
runs, so the member must not be read from there on.

## log-excerpt-per-request

A question about the log gets the current excerpt folded into just this request (see
LlamaClient::analyze_log); history_ itself only ever keeps the plain question, or every later
turn would resend the whole excerpt.

## only-answers-in-history

Only a real answer belongs in the history; an error message would be sent back as if the
assistant had said it. The answer goes in complete with its proposal block, so the model sees
in the next turn what it proposed.

## hint-in-transcript

The hint names a directory path and does not fit the one-line status label; the read-only
transcript wraps it and lets the user copy the path.

## ctrl-enter-sends

Plain Enter inserts a line break in the multi-line field, so sending needs a modifier.

## focus-only-when-shown

The answer can arrive after the user closed the chat, which hides the dialog: moving the
keyboard focus into a hidden window would take it away from whatever the user is typing in
by then - a modal dialog included.

## hidden-not-destroyed

Hidden rather than destroyed: the conversation stays available when the user opens the chat
again, and a request still running has a live window to deliver its answer to.
