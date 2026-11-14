# FileDelete

Module comment for FileDelete.h / FileDelete.cpp.

## delete_file

Deletes according to the configured DeleteMode: permanently, or by moving the file into the
recycle bin under a timestamped name. Every deletion of data that belongs to the user goes
through here; our own temporary files (backups, half-copied targets, the config temp file) are
removed directly, they have nothing to recover.

## recycle-bin-layout

relative_path() strips the root ("C:\" resp. "/") so the origin's directory structure is rebuilt
below the bin; the timestamp prefix keeps files deleted repeatedly from the same place apart.

## free_bin_item

The timestamp has millisecond resolution, so a file that is deleted, written again and deleted
again within one millisecond would get the name of its first deletion and replace it in the
bin. The time is moved on by a millisecond until the name is free. The bin itself refuses a
taken name as well (RecycleBin.md#add_item-never-replaces); should another thread take the name
in between, the deletion fails and the file stays where it is instead of costing the earlier
one.
