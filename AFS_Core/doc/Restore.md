# Restore

Module comment for Restore.h / Restore.cpp.

## restore

Finds what the synchronisation has put aside and gives it back: files it deleted into the
recycle bin (FileDelete.md#delete_file) and earlier versions the father-son principle kept in
the target (SyncBase.md#father_son_backup). Until now both could only be fetched by hand, by
someone who knew the bin's layout and the ".bak.<n>" names. Giving back never overwrites and
never deletes a file the user has.

## Restorable

One file that can be given back. `stored` is where its bytes lie now, in the bin or beside the
target file. `home` is the file it was, named at the place the user works with it (#home_of);
its directory is where a restore puts it unless the user names another. `time` is the moment of
deletion for a deleted file and the last modification for an earlier version, which is the
moment that version was current.

## find

The bin first, then the targets of all items that keep versions, newest first. Meant for a
worker thread: reading a large target tree takes time, and `stop` ends the search for versions
early when the caller no longer wants the result. The items are passed in instead of read from
the configuration so the caller can take its snapshot on its own thread.

## stamped

A name the bin made: "YYYY-MM-DD_hh-mm-ss-mmm_<name>" in local time (Util.md#with_timestamp_prefix).
Names without that prefix are not ours and are not offered.

## stamped_origin

The path a bin item had before deletion, still without its root. Normally the file name carries
the prefix. When a whole directory was put into the bin the prefix sits on that directory and
the files below kept their names; then the first prefixed directory on the way is taken. The
file name is asked first so a user directory that merely looks stamped does not win over it.

## Place

Target and source of an item whose both sides are local directories. in_source() translates a
file of the target into the file of the source it mirrors, following the item's type: a tree
maps every depth, a directory only its own files, a single file only itself. Paths are compared
without their root, because the bin's layout has none (FileDelete.md#recycle-bin-layout).

## home_of

A file deleted from a target is offered for the source, not for the target. The target is a
copy: a file put back there is deleted again by the next pass of an item that mirrors
deletions, while a file put back into the source is simply synchronised. Of nested targets the
deepest decides. A file no item covers goes back where it was; on Windows that place is unknown
because the bin does not record the drive, so the path stays relative and the caller has to ask
(#place_known). A remote source cannot be written through the file system, so such an item
does not translate and its files go back into the target.

## place_known

Whether `home` names a real place. False only for a bin item on Windows that no item covers.

## versions

Earlier versions are looked for only in items that keep them: without the father-son principle
a ".bak.1" is an ordinary file of the user (SyncBase.md#father_son_backup). A tree is searched
at every depth, the other types in the target directory alone, as the synchronisation writes
them. Remote targets are not searched.

## put_back

Puts the file into `directory`, creating it if needed, and returns where it lies now. A deleted
file is moved out of the bin, so it is no longer offered afterwards. An earlier version is
copied and stays in the target, because it is still the backup of that moment.

## free_name

The file gets its own name when that is free. Otherwise the name is extended by the item's
time, "bericht (2026-11-01 14-30).txt", and after that by a counter. This is what keeps a
restore from ever replacing a file: the user sees both and decides. A path whose state cannot
be read counts as free, so the loop always ends; the move or copy then fails on its own.

## copy_version

The copy gets the current time as its modification time. With its old time a version restored
under the original name would look outdated to the next pass, and a one-way item would leave
the target on the newer file the user just wanted to get rid of.
