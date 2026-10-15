# SyncOnceTest

Module comment for SyncOnceTest.cpp.

## EinmalSynchronisierenKopiertDateiInsZiel

The first end-to-end test and the proof that the sandbox works: it creates
the synchronisation through the dialog with a type other than the default, runs it through
the menu and reads the result from a SandboxTree that removes itself. The type is
"Verzeichnis": the sandbox's own source directory is left alone, so GuiTest::TearDown still
finds it untouched. Both menu items are checked afterwards because the run ends in two steps
(see SyncActions.md) and a lock left behind would block the next test's run.

## dir-versus-tree

The types "Verzeichnis" and "Verzeichnisbaum" differ only in the subdirectories, so the pair
of tests shares a layout and checks the one entry that must be missing for the flat type.

## nothing-to-do

The count pass counts every file the synchronisation considers, not the ones that differ, so
a second run over an unchanged tree compares and copies nothing without a message. The
unchanged write time of the target is the proof that nothing was copied. The run waits out
the item's pause first, see below.

## pause

Every synchronisation has a pause (the sync cycle, 1 s by default) and a run inside it skips
the item. "Einmal synchronisieren" twice in quick succession therefore counts nothing and
shows the "Info" box - the same box as for an inactive item, so the box is the only thing the
user learns. The test pins that behaviour down; whether the text should mention the pause
is a question for the plan's open points.

## start-without-sync-item

Start with an empty configuration ends in the "Achtung" box on the first timer tick. The test
stops the run right after and expects no second box: Stop has to leave the continuous mode
even though no run was ever active (SyncInterface.md#stop-leaves-continuous-mode).

## proposed-command

A command the assistant proposes runs exactly the menu command, so "Einmal synchronisieren"
must sync like the menu item does. A command whose menu item is disabled - stopping when
nothing runs - must not run, but tell the user why, instead of doing nothing on a click.
apply_proposal() stands in for the button of the chat, which only a model can make appear.
