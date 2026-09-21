# SyncOnceTest

Module comment for SyncOnceTest.cpp.

## EinmalSynchronisierenKopiertDateiInsZiel

The first test of the plan's phase 1 (GUI-Testplan.txt) and the proof of phase 0: it creates
the synchronisation through the dialog with a type other than the default, runs it through
the menu and reads the result from a SandboxTree that removes itself. The type is
"Verzeichnis": the sandbox's own source directory is left alone, so GuiTest::TearDown still
finds it untouched. Both menu items are checked afterwards because the run ends in two steps
(see SyncActions.md) and a lock left behind would block the next test's run.

## dir-versus-tree

The types "Verzeichnis" and "Verzeichnisbaum" differ only in the subdirectories, so the pair
of tests shares a layout and checks the one entry that must be missing for the flat type.

## nothing-to-do

The second run over an unchanged tree has nothing to count and ends in the "Info" box before
any copying. The box is answered by the watcher; the test reads its text back, because the
box is the only way the user learns why nothing happened.

## start-without-sync-item

Start with an empty configuration ends in the "Achtung" box on the first timer tick. The test
stops the run right after and expects no second box: Stop has to leave the continuous mode
even though no run was ever active (SyncInterface.md#stop-leaves-continuous-mode).
