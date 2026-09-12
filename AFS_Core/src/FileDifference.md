# FileDifference

Module comment for FileDifference.h / FileDifference.cpp.

## ssh-error-is-not-absence

Connection/auth failure, not a confirmed-absent file: must not be mistaken for "source deleted",
or SyncFile deletes the destination (Fehler 2) the moment delete_in_target() is enabled.
