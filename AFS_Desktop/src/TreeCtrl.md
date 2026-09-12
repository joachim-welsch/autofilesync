# TreeCtrl

Module comment for TreeCtrl.h / TreeCtrl.cpp.

## separator-detected-from-path

SSH paths ("account:/remote/...") and local Unix/macOS paths use '/' exclusively and never
contain '\'; only Windows local paths do. Detect from the actual path instead of hardcoding '\',
or every non-Windows path shows up as a single flat leaf instead of a folder hierarchy.

## empty-segments-removed

A leading/trailing/doubled separator (e.g. a Dir item's path ending in '/') otherwise produces
an empty segment, showing up as a spurious unnamed node in the tree.

## leading-separator-restored

A leaf's registered path must match the SyncItem's source()/target() verbatim (Config::find()
compares them for string equality to enable the tree view's context menu), so a leading
separator stripped from strs above has to be put back here - only the empty segments it produced
were the problem.
