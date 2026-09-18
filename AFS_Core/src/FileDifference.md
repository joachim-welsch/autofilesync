# FileDifference

Module comment for FileDifference.h / FileDifference.cpp.

## ssh-error-is-not-absence

Connection/auth failure, not a confirmed-absent file: must not be mistaken for "source deleted",
or SyncFile deletes the destination (Fehler 2) the moment delete_in_target() is enabled.

## ssh-content-fallback

A content comparison needs both files locally, so it cannot run for an SSH path. Skipping it left
an item whose only criterion is the content - the default for a new item - with nothing to
compare at all: every file that exists on both sides came out Equal, so after the first copy no
change ever reached the other side again. The content criterion therefore degrades to the size
for SSH paths, once in the constructor, which keeps compare_metadata() and the content check
consistent and makes compare_content() unreachable for SSH.

Size and time both, since the transfer carries the modification time of the file it copies (see
SyncFileSsh.md#transfer-keeps-the-modification-time) - a remote copy no longer looks newer than
the original it came from, so the time is the criterion that also notices an edit which leaves
the size unchanged.

## remote-time-conversion

file_time_type does not count from the Unix epoch - libstdc++ places its epoch in the 22nd
century, the Windows one in 1601 - so building one directly from the seconds an SFTP server
reports produced a time roughly two centuries off. Every remote file therefore compared as newer
than every local file, whatever its real age. util::to_file_time() converts through the clocks
instead.

## second-resolution

SFTP reports the modification time in whole seconds while a local file carries sub-second
precision, so a file and its own copy differ by up to one second. Comparing the seconds is what
keeps that difference from looking like a change and sending the file across on every pass.

## content-conflict

Two-way sync copies the newer file over the older one. When time and size are
equal but the content differs, neither side is the newer one, and copying in
either direction would destroy an edit. The result is therefore its own value,
which SyncFile leaves untouched and reports as an error.
