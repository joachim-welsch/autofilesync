# SettingsRecycleBinTest

Module comment for SettingsRecycleBinTest.cpp.

## RestoredState

The recycle bin path is process wide state that GuiTest::TearDown() does not cover, and the
deleted file has to be put back where the sandbox snapshot expects it - both must happen even
when an assertion ends the test early.

## manual-restore

Puts the sandbox back the way GuiTest::TearDown() expects it; restoreItem() cannot be used here
because it restores relative to the working directory, not to the absolute place the file came
from.
