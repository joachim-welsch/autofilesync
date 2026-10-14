# GridCtrl

Module comment for GridCtrl.h / GridCtrl.cpp.

## kMinRowLabelSize

Lower bound for the name column (the row label) so that short names do not leave it needlessly
narrow; longer names widen it beyond this through wxGRID_AUTOSIZE in addDataItems().

## row-label-alignment

wxGrid centres row labels by default; a name longer than the column then gets clipped on both
sides instead of staying readable from the start.

## autosize-row-label

Fits the name column to the longest name currently shown, so that it always stays fully visible
instead of being clipped.

## selection-restored

Rebuilding rows from scratch drops any selection/cursor position; restore it by name so editing
(or deleting) an item doesn't force the user to re-click it before they can edit/delete it
again.

## copy-written-back

found is a copy; write it back under Config's lock via the original name (the dialog may have
renamed it).

## empty-source-or-target-rejected

SyncItemDialog disables its OK button while source/target are empty (onUpdateOkButton); the
grid's inline cell edit has no such gate, so it must reject the same case explicitly instead of
silently saving it.
