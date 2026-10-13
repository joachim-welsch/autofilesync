# ChatDialogTest

Module comment for ChatDialogTest.cpp.

## openChat

The chat is a modeless dialog owned by the main window, created on the first "Assistent → Chat"
and re-shown afterwards; its controls are looked up by id across all windows because the dialog
is not a child of the main window's client area.

## HiddenChat

Waits for a pending answer before it hides the dialog: a reply that arrives while the next
test runs would re-enable the controls under that test's feet. Hidden, not closed - the dialog
is meant to keep its transcript (ChatDialog.md#hidden-not-destroyed).

## hidden-not-destroyed

Closing the chat hides it and the menu shows it again with everything still in it; the
unfinished text in the input is the visible proof that the dialog was kept.

## ChatStrgEingabeSendet

Ctrl+Enter is the keyboard way to send; the question in the transcript and the emptied input
are what the user sees at once, the answer follows in a minute or two, which is why this is a
SlowGuiTest.

## ChatAntwortErscheintWaehrendSieEntsteht

The answer shows up in the transcript while the dialog still waits, and grows afterwards; a
counting answer is long enough that its first piece arrives well before the last. The speaker
line is counted because onReply() must close the streamed turn, not append the answer again.
Skipped without a model: the sandbox moves HOME, so only AUTOFILESYNC_LLAMA_MODEL can name one.

## no-model

Without a model the reply is an immediate "Fehler:" turn and the controls are enabled again
before the test looks; either state means the question was sent.
