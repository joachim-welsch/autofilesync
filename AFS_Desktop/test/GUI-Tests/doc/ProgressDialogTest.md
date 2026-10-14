# ProgressDialogTest

Module comment for ProgressDialogTest.cpp.

## MANY_FILES

Enough files that the copy run outlives the moment the test looks at the dialog and clicks
Cancel: a single file is copied before the dialog's 100 ms timer has shown it once. Small
files, because the time goes into creating files, not into writing bytes - 1500 of them take
a few seconds to copy and the SandboxTree a second to build.

## FortschrittsdialogZeigtAktuelleDateiUndSchliesstSich

The dialog is found among the top-level windows (see SyncActions.md#progress_dialog) while
the run is on, and the file label is read as described in SyncActions.md#current_file_shown.

## AbbrechenImFortschrittsdialogBeendetDenLauf

Cancel is proven by what the user sees afterwards: fewer files in the target than in the
source, the dialog gone and the menu free again. The click runs the dialog's own wait loop
(ProgressDialog.md#cancel-waits-here), so the test's click returns only once the run has
stopped. This test found that Cancel used to let the run complete, see
SyncInterface.md#reset-before-the-next-run.
