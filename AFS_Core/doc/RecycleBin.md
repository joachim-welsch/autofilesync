# RecycleBin

Module comment for RecycleBin.h / RecycleBin.cpp.

## RecycleBin

Items are addressed by a path relative to the current working directory. The path is mirrored
inside the bin, so restore_item() can put the file back where it came from.

## add_item-outside-cwd

Same, but for a source that lies outside the current working directory: it is stored in the bin
under the relative path 'item', which lets the caller pick the name there (a timestamp prefix,
say).

## take_item

Moves an item out of the bin to any destination, which restore_item() does for the place the
relative path names. An existing destination is refused: the bin gives files back and must
never be the reason another one is lost.

## check_relative

An absolute path or a ".." component would place the item outside path_, which the bin must
never touch.

## move

rename() fails across filesystem boundaries (EXDEV), which happens as soon as the bin lives on
another drive than the file. The entry is then copied and the original removed. The copy has to
do what the rename did for free: take a whole directory along, keep a symlink a symlink, and
keep every modification time - a file restored from the bin with the time of its restoring
would look freshly edited to the next sync and overwrite the other side.

## list_items

Reads the directory instead of a member list, so items survive a restart.
