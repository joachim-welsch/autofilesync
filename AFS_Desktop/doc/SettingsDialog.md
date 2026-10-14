# SettingsDialog

Module comment for SettingsDialog.h / SettingsDialog.cpp.

## enable_recycle_bin_path

The path has no meaning while files are deleted permanently; showing it as editable would
suggest otherwise.

## on_pick_recycle_bin_path

The directory need not exist yet: RecycleBin::addItem() creates it on demand, so an empty entry
falls back to the bin's default path instead of failing.

## language_index

Entry 0 of the choice is the operating system's language, the others follow
translation::LANGUAGES. A code in the configuration that is not offered (written by a newer
version, or by hand) shows as entry 0, and is kept unless the user picks another entry: OK
alone must not overwrite a choice the dialog merely cannot display.
