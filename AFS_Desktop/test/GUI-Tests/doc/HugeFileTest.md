# HugeFileTest

Module comment for HugeFileTest.cpp: "Einmal synchronisieren" over one huge file, with the
progress dialog as the user sees it meanwhile.

## HUGE_FILE_BLOCKS

512 MiB: large enough that the copy and the verification read that follows it
(SyncFile.md#ssh-skips-verification-only) outlive the dialog's 100 ms timer and the test's look
at the dialog, so the file name is seen while the copy is on. Not larger, because the sandbox
lives in the temp directory, which is a RAM-backed tmpfs on many Linux systems, and source and
copy exist side by side there. The file stays below 4 GiB on purpose; a size above it would
test the 32-bit limits of the counters, which this test does not.

## block

The content is generated, not stored: each 1 MiB block is filled with its own index, and the
copy is checked block by block against the same generator. That keeps two 512 MiB strings out
of memory and still catches a truncated copy, a missing block and blocks in the wrong order.

## EinmalSynchronisierenKopiertRiesigeDateiMitFortschrittsdialog

The dialog is checked for what a user looks at during a long copy: the title, the name of the
file being copied, a Cancel that is enabled and a bar that moves while the file is copied.
After the run the copy must be complete, carry the source's write time and leave no .bak
behind.

## bar_values

The bar is sampled every 10 ms for as long as the dialog is shown. A single look could fall
before the first timer tick or after the last one; the samples have to contain a value
between empty and full (the bar moved inside the file, ProgressDialog.md#file-progress),
rise monotonically and end full. The dialog's timer ticks every 100 ms; the copy of 512 MiB
takes a few hundred milliseconds even in RAM, so the samples see at least one step inside
the file, and the verification read that follows shows the full bar before the dialog goes.

## AbbrechenWaehrendRiesigerDateiHinterlaesstKeineHalbeDatei

A local copy stops at the next block after Cancel (SyncFile.md#copy_local_file) and the
partial target is removed; a click that lands after the last block leaves the complete copy.
What the user must be able to rely on is that the target never holds a partial file, so the
test accepts either outcome, no file or the complete one, and rejects anything in between,
including a .bak left over from an interrupted step.
