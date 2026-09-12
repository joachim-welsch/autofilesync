# ListCtrl

Module comment for ListCtrl.h / ListCtrl.cpp.

## selection-restored

Rebuilding items from scratch drops any selection; restore it by name so editing an item doesn't
force the user to re-click it before they can edit/delete it again.

## copy-written-back

found is a copy; write it back under Config's lock via the original name (the dialog may have
renamed it).
