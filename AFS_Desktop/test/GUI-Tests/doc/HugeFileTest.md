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
file being copied, a bar whose range is the one file of the run and a Cancel that is enabled.
The bar's value is not checked: the per-file progress of ProgressDialog (set_range,
set_progress) has no caller, so the bar stands at 0 of 1 for the whole copy and only fills
when the run ends. After the run the copy must be complete, carry the source's write time
and leave no .bak behind.

## AbbrechenWaehrendRiesigerDateiHinterlaesstKeineHalbeDatei

A local copy runs through fs::copy_file and cannot be interrupted, so Cancel during the copy
ends after the file, not inside it; what the user must be able to rely on is that the target
never holds a partial file. The test therefore accepts either outcome, no file or the complete
one, and rejects anything in between, including a .bak left over from an interrupted step.
