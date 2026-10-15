# ChatDialogTest

Module comment for ChatDialogTest.cpp.

## REPLY_TIMEOUT_MILLISECONDS

Ten minutes, like ChatLogQuestionTest: the chat dialog is hidden between tests, not destroyed,
so every later test's question carries the whole history of the earlier ones, and a changed
configuration makes the model decode that prefix again. On a laptop CPU that throttles under
sustained load (measured 96 °C after an hour of tests) one answer took over three minutes; a
tighter limit fails the test for the hardware, not for the code.

## open_chat

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
line is counted because on_reply() must close the streamed turn, not append the answer again.
Skipped without a model: the sandbox moves HOME, so only AUTOFILESYNC_LLAMA_MODEL can name one.

## ChatKenntDieAnzahlDerSynchronisationen

Three synchronisations are added through the GUI, then the assistant is asked how many there
are: the only way it can know is the configuration in its prompt (ConfigSummary.md). A number
is the one thing a small model answers reliably when told to answer with the number alone;
the names of the items would be a gamble. GuiTest::TearDown restores the configuration.

## ChatVorschlagLegtSynchronisationUeberDenDialogAn

The whole path of a proposal: the question names source, target and type, the answer must
carry a block (the button appears), and the button opens the ordinary "add synchronisation"
dialog prefilled - the source field is read there as proof - which is confirmed with OK like a
hand-made item. The paths are the sandbox's, so the dialog's validation accepts them and
GuiTest::TearDown restores the configuration.

## block-not-shown

The JSON block is for the parser, not the user: the transcript must not contain a code fence,
neither from the streamed pieces nor from the final answer.

## proposal-described

The answer part of the transcript (from the last assistant label on) must name the proposed
item and its target even when the model wrote nothing but the block: that is the sentence from
SyncProposal.md#describe, and without it the user sees an empty answer.

## deletion-proposal

The same test deletes the item it created through the chat: the button must now carry the
delete label, pressing it asks the usual confirmation question (answered yes by the watcher),
and afterwards the item is gone. Deleting the item the test created keeps the configuration as
it was even if the teardown's restore were to fail.

## last-answer-only

The dialog is hidden, not destroyed, between tests, so its transcript still holds the earlier
tests' answers - the counting answer of ChatAntwortErscheintWaehrendSieEntsteht contains every
digit. Only the text after the last speaker line is this test's answer.

## no-model

Without a model the reply is an immediate "Fehler:" turn and the controls are enabled again
before the test looks; either state means the question was sent.

## AssistentHatEinEigenesMenue

Regression test: following the platform guidelines, the chat once moved into the Help menu, and
users took the assistant for removed. The menu of its own is what they look for.

## ChatNeuesGespraechLeertDenVerlauf

The chat window outlives each test, so the test makes its own round first. Without a model the
assistant answers at once with an error, which is a round all the same; with a model the answer
would take minutes, and the test skips. Afterwards only the hint shown at start may be left.

## HilfebuchLiefertDemAssistentenSeineAbschnitte

The assistant reads the help from the packed help book, the same file the help window shows.
The Hintergrunddienst page proves that the zip entries were found and their UTF-8 title came
through; a missing book must leave the assistant without help, not fail.

## ChatBeantwortetHilfefrageMitDerHilfeseite

The question uses the help's own words, so the "Globale Einstellungen – Sprache" section goes
along, and the instruction asks the model to name the page. Without the help the model knows
neither the dialog's name nor that a language choice exists. Runs in German: the help and the
page name are German, and an answer in another language could translate the name.
