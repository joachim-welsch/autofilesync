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
