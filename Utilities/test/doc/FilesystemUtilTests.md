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

## COMPARE_BLOCK

files_equal() reads both files in blocks of this size. Contents of several blocks make sure the
loop goes on behind the first block, and a size that is an exact multiple of the block hits the
iteration in which both reads return nothing.

## RemoveFileFailsForANonEmptyDirectory

fs::remove() throws here instead of answering false; remove_file() has to turn that into false
without deleting anything.

## WriteUtf8FileWritesTheBytesAndThrowsWhereItCannotWrite

The configuration and the proof files go through this; the bytes on disk are UTF-8 without a
BOM, and a path that cannot be opened throws instead of silently writing nothing - the caller
reports the failure (ConfigPathTest.WriteFailureDoesNotCorruptExistingConfig relies on that).

## VolumeExistsLooksForTheVolumeInTheMountTable

On Unix the volume name is searched in the mount table as a substring of a device or mount
point and the drive name is ignored. Linux reads /proc/mounts, macOS asks getmntinfo() because
it has no /proc; /proc is mounted on every Linux system, devfs on /dev on every macOS system,
the made-up name on none.

## WritePrivateFileIsOwnerOnlyFromTheStart

The secrets of the program (FilesystemUtil.md#private-files) are written with the bytes as given, NUL
included, and on POSIX with mode 0600 - also when the file existed before with wider
permissions, which an earlier version could have left behind. A missing directory is a plain
false, so the caller logs it as PasswordCrypto and DeviceIdentity do.

## RemoveFileReturnsWhetherSomethingWasActuallyRemoved

Fehler 5 (round 6): remove_file() discarded fs::remove()'s own return value and always reported
success regardless of whether anything was actually deleted - e.g. for an SSH "account:/path"
string misinterpreted as a bogus local path, or any other path that simply didn't exist.
