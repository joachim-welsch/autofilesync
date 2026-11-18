# RecycleBin

Module comment for RecycleBin.h / RecycleBin.cpp.

## RecycleBin

Items are addressed by a path relative to the current working directory. The path is mirrored
inside the bin, so restore_item() can put the file back where it came from.

## add_item-outside-cwd

Same, but for a source that lies outside the current working directory: it is stored in the bin
under the relative path 'item', which lets the caller pick the name there (a timestamp prefix,
say).

## add_item-never-replaces

A name already taken in the bin is refused. The rename that moves the item would otherwise
replace the file lying there, and the bin would lose exactly what it was asked to keep. The
caller picks the name, so the caller has to pick a free one (FileDelete.md#free_bin_item);
has_item() tells it.

## add_refusal

add_item() with the reason for a failure, for the report of a deletion that did not happen
(FileDelete.md#delete_refusal): FileNotExist when the source is gone, the reason the system
gave when the bin's directory could not be created or the move failed, and a write error for a
taken name or a path the bin refuses. FileNotExist is only said when the system says so: a
source that cannot be looked at, behind a directory without search permission, gets that
reason instead, because the caller reads FileNotExist as "nothing left to delete"
(SyncBase.md#deleted).

## read-only-source

A file on a read-only drive cannot be moved into the bin, and the attempt was costly: the
rename failed for the other drive, the whole file was copied into the bin, its original would
not go, and the copy was removed again - once per file and pass. So the drive is asked first
(FilesystemUtil.md#read_only_filesystem) and the answer is the refusal. A source that is
missing is still FileNotExist: there is nothing left to delete, whatever the drive allows.

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

## move-takes-the-copy-back

The copy that stands in for a rename is only a move once the original is gone. A copy that
broke off, and a copy whose original would not go, is removed again: otherwise the bin kept one
more copy with every attempt while the file stayed where it was, and a restore gave back a
file that was never deleted. The exception is a directory whose removal failed: part of it may
already be gone, and then the copy is the only place that still has everything.

## list_items

Reads the directory instead of a member list, so items survive a restart.
