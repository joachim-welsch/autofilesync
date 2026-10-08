# SettingsDialog

Module comment for SettingsDialog.h / SettingsDialog.cpp.

## enableRecycleBinPath

The path has no meaning while files are deleted permanently; showing it as editable would
suggest otherwise.

## onPickRecycleBinPath

The directory need not exist yet: RecycleBin::addItem() creates it on demand, so an empty entry
falls back to the bin's default path instead of failing.
