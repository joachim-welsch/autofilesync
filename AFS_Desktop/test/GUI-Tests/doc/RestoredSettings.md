# RestoredSettings

Module comment for RestoredSettings.h.

## RestoredSettings

Settings and the recycle bin path are process-wide state that GuiTest::TearDown does not put
back (it re-reads the file, but RecycleBin keeps its own path), so a test that changes them
saves and restores them around its scope, whether it passed or failed.
