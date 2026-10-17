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

The toolbar replaces the Start tool by a Stop tool in place (Toolbar::set_started), so the
presence of the tool ids is what the user sees. The tools are activated through the toolbar's
handler, see UserActions.md#activate_toolbar_tool.

## StatuszeileZeigtZustandDerSynchronisation

The second status field is read as the user reads it. A 60 s cycle makes the run pause right
after its first pass, so "Pausiert" is what the field has to show then; the seconds are not
pinned down because the tick that refreshes the field is not synchronised with the pass.
