# Canvas

Module comment for Canvas.h / Canvas.cpp.

## select_path

Selects and scrolls to path's node if this side's tree has one for it. Returns false (and leaves
the selection untouched) if it doesn't.

## on_item_left_clicked

A left click on item: if it is a SyncItem's own source/target root, reveal the matching path on
the other side instead of just clearing its selection - the two sides then always point at the
same SyncItem, which is also what MainWindow::find_sync_item() relies on.

## on_size

Lays heading_ and tree_ctrl_ out top-to-bottom. Bound to EVT_SIZE rather than done in an
overridden SetSize(): the splitter window resizes its panes through its own internal layout, not
through calls this class would see, and GetClientSize() only reports the final size once the
toolkit's own resize has actually settled.

## no-inline-rename

No wxTR_EDIT_LABELS: nothing binds EVT_TREE_BEGIN/END_LABEL_EDIT, and Tree::path() resolves by
wxTreeItemId, not label text, so an inline rename would just show a misleading label without
renaming anything.
