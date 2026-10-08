# ScopedRecycleBin

Module comment for ScopedRecycleBin.h.

## ScopedRecycleBin

A sync that deletes in the target moves the file into the recycle bin. A test of
it would fill the bin of the user - or of the test home - with one file per run.
The test therefore gets a bin of its own in the temp directory; the destructor
points RecycleBin back to the previous bin and deletes this one.
