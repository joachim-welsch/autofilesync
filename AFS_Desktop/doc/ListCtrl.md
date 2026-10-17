# ListCtrl

Module comment for ListCtrl.h / ListCtrl.cpp.

## selection-restored

Rebuilding items from scratch drops any selection; restore it by name so editing an item doesn't
force the user to re-click it before they can edit/delete it again.

## copy-written-back

found is a copy; write it back under Config's lock via the original name (the dialog may have
renamed it).

## items-only

The list runs in wxLC_LIST mode and has no columns of its own, so only the items are cleared.
ClearAll() also deletes the columns; on Windows a list-mode control keeps one internal column
that cannot be deleted, and wxWidgets logged a failed ListView_DeleteColumn on every update.
