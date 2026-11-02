# FilesystemUtilTests

Module comment for FilesystemUtilTests.cpp.

## FileinfoLeavesTheOutputUntouchedForAMissingFile

The temp directory lies on a mounted drive, so the answer has to be FileNotExist and not
DriveNotMounted - the synchronisation waits for a drive but skips a missing file.

## EveryFileInfoResultHasItsOwnMessage

The message is all the user gets to see of a failed fileinfo(). Two results sharing one text - a
missing translation key falls back to the same "unknown error", say - would hide the cause.

## TouchMovesTheWriteTimeToNow

The reference lies two seconds in the past: filesystems with a coarse timestamp resolution round
the new write time down, which may put it slightly before a reference taken right at the call.

## HomeDirFallsBackToThePasswordDatabase

A service started by systemd or cron may run without HOME. The recycle bin and the configuration
live below the home directory, so an empty answer would move them into the working directory.

## MountRootOfAMissingPathIsThatOfItsExistingParent

A target directory that is still to be created has to be assigned to the drive it will be created
on, otherwise the check for a mounted drive fails before the first synchronisation.

## ProcIsAMountPointOfItsOwn

/proc is the one mount point below the root that exists on every Linux system, including
containers and build servers, so the test does not depend on the machine's partitioning.

## ASiblingWithTheSamePrefixIsNotCovered

A mount point covers a path only at a component boundary: a plain prefix comparison would assign
/procfoo to /proc, and "/media/usb2" to a drive mounted at "/media/usb".

## PlainEntryNameRefusesSeparatorsAndDotEntries

The names an SFTP server may list (FilesystemUtil.md#plain_entry_name): ordinary names pass,
also one that merely starts with two dots; the dot entries, an empty name and every name with a
slash, a backslash or a NUL are refused, since each of them would leave the directory it was
listed in once appended to a path.
