# ContinuousSyncTest

Module comment for ContinuousSyncTest.cpp.

## RunningSync

Stops a continuous run when the test leaves its scope, whether it passed or failed: a run
left behind would copy into the next test's sandbox and keep the Start menu locked for it.

## StartUndStoppLaufenMehrerePassesUndStoppenSauber

A second pass is proven by a file written after the first pass copied - the only observable
difference between "ran once" and "keeps running". After Stop the test waits longer than
pause plus timer tick before it looks for the third file, so a run that ignored Stop would
have had its chance to copy.

## ToolbarStartUndStoppSchaltenUm

The toolbar replaces the Start tool by a Stop tool in place (Toolbar::setStarted), so the
presence of the tool ids is what the user sees. The tools are activated through the toolbar's
handler, see UserActions.md#activateToolbarTool.
