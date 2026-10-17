# PreviewTest

Module comment for PreviewTest.cpp.

## preview

"Vorschau…" from the menu, waited out like a single run: the pass ends on a timer tick
(SyncInterface.md#preview), and when the dialog's action chose "Jetzt synchronisieren" the
single run that follows is waited out as well, because wait_until_sync_finished() only
returns once no pass is active.

## VorschauZeigtKopienUndLoeschungenUndAendertNichts

The dialog is modal, so its rows are read inside the ModalDialogWatcher action and checked
after it closed. One file to copy and one to delete cover the two actions the user needs to
tell apart, and the file names are expected bare: the copy is named relative to the source,
the deletion relative to the target (PreviewDialog.md#shown_path); the listing of the sandbox
before and after is the proof that the preview touched nothing, and the three start commands have to be usable again afterwards (the counterpart of
SyncOnceTest.md#EinmalSynchronisierenKopiertDateiInsZiel).

## JetztSynchronisierenInDerVorschauFuehrtDenLaufAus

The run after the preview is the ordinary single run; the copied file is the proof it ran.
The run's own "nothing to do" box cannot appear here because the preview just found work
and the item's pause (1 s by default) is not consulted by the preview but has long elapsed
for a fresh item.

## VorgeschlageneVorschauZeigtDenDialog

The assistant may propose the preview like any other menu command
(SyncProposal.md#Command); apply_proposal() stands in for the chat's button, as in
SyncOnceTest.md#proposed-command. The file must still be missing in the target afterwards:
the proposal shows, it does not run.

## VorschauMitNichtErreichbaremZielWarntStattNichtsZuTun

The screenshot that started this: two synchronisations onto an external disk that was not
plugged in, and the preview announced that source and target match. The target directory
simply does not exist here, which is unreachable for a local path (CheckPath.md#reachability-rules).
The one box must name the item (SyncInterface.md#preview-unreachable) and not contain the
"nothing to do" sentence; the error line with the paths may follow in the same box, which is
why the test checks the start of the message rather than all of it.

## VorschauOhneUnterschiedMeldetNichtsZuTun

After a run the sides match, and the preview must say so in a box rather than show an empty
list. The preview ignores the pause (Synchronize.md#preview-ignores-the-pause), so it can
follow the run at once without a wait - which is what this test also pins down: within the
pause a single run would show "nothing to do" for a different reason.
