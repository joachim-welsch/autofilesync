# SyncOnceTest

Module comment for SyncOnceTest.cpp.

## EinmalSynchronisierenKopiertDateiInsZiel

The first test of the plan's phase 1 (GUI-Testplan.txt) and the proof of phase 0: it creates
the synchronisation through the dialog with a type other than the default, runs it through
the menu and reads the result from a SandboxTree that removes itself. The type is
"Verzeichnis": the sandbox's own source directory is left alone, so GuiTest::TearDown still
finds it untouched. Both menu items are checked afterwards because the run ends in two steps
(see SyncActions.md) and a lock left behind would block the next test's run.
