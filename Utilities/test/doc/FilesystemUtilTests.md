# FilesystemUtilTests

Module comment for FilesystemUtilTests.cpp.

## FileinfoLeavesTheOutputUntouchedForAMissingFile

The temp directory lies on a mounted drive, so the answer has to be FileNotExist and not
DriveNotMounted - the synchronisation waits for a drive but skips a missing file.

## FileinfoTellsAMissingDirectoryFromAMissingFile

DirectoryNotExist is the answer whenever the directory the file should lie in is not a
directory: it is missing, a level above it is missing, or a plain file bears its name. The
output stays untouched as for a missing file.

## FileinfoTellsARefusedFileFromAMissingOne

A directory without any permission hides whether a file in it exists. PermissionDenied is the
answer for the file that is there, for the one that is not, and for one a level deeper: none of
them is known to be missing. Unix only and skipped as root, like every test that locks
something with chmod. The permission is given back before the assertions, so that a failing
test can still be cleaned up.

## FileinfoOfAFileThatMayNotBeReadStillAnswers

Size and write time come from the directory entry, not from the content. A file without read
permission is therefore no PermissionDenied here; the copy is what fails
(LocalPermissionTests.md#UnreadableSourceKeepsTheOldTargetAndReportsTheError).

## ReasonForFailureTellsLockedFromRefusedFromMissing

The mapping from the system's error to the reason the user reads. A locked file cannot be
produced on Unix, where locks are advisory and a file another program holds open still reads;
so the codes that mean "in use" are fed in directly. On Windows they are the two sharing and
lock violations, which the generic conditions would lump together with "permission denied".

## CompareContentTellsDifferentFromNotReadable

"Not equal" has two meanings that must not be mixed: both files were read and differ, or one
of them could not be read. Only the first is a difference. The refusal carries the reason and
the file it belongs to, here a missing one.

## FileThatDoesNotOpenIsRefusedNotDifferent

A file without read permission is the Unix stand-in for a file another program has locked: it
is there, its size and time can be asked for, and opening it fails. read_refusal() and
compare_content() have to say so and name the right one of the two files.

## CreateDirectoryRefusalSaysWhyADirectoryIsNotThere

OK means the directory is there afterwards, created with every missing level or found.
CannotCreateDirectory is the answer when no better reason is known, as with a plain file that
bears the name of the directory or of a level above it; the file stays what it was, and the
error code goes back to the caller for the text. A directory that refuses the creation is
PermissionDenied (second test, Unix only).

## FailureTextNamesKnownReasonsAndFallsBackToTheSystem

The text behind an error code for the report. Reasons the program knows are said in the
user's language; for everything else the system's own text is better than "unknown error" -
"No space left on device" tells the user what to do.

## FreeSpaceAnswersForADirectoryAndFollowsTheTestValue

free_space() answers for the drive a directory lies on, has no answer for a directory that is
not there - which callers must not take for "no room" - and gives the value a test set until
the test takes it back (DiskFullTests.md#FreeSpace).

## ReasonForWriteFailureKeepsKnownReasonsAndCallsTheRestAWriteError

A failed write must not be reported as a read error, and a reason the system does name -
no right, no room, a lock, a missing file - must not disappear behind the general one. The
busy device is a lock on Unix only; Windows does not count it as one, so there it is one of
"the rest" and has to come out as a write error.

## WriteTimeRefusalSetsTheTimeOrSaysWhyNot

The real path: the time arrives, and a missing file is said to be missing.

## WriteTimeErrorForTestRefusesWithoutTouchingTheFile

The override the sync tests rely on (WriteErrorTests.md#WriteTimeError): while it is set the
file keeps its time, and once it is taken back the next call works again.

## PermissionsRefusalSetsTheRightsOrSaysWhyNot

The real path: the write bit goes, and a missing file is said to be missing. The bit comes
back at the end so that TearDown can remove the file on every platform.

## PermissionsErrorForTestRefusesWithoutTouchingTheFile

The three answers the override can provoke: the general one, a reason the system names, and
"not supported", which is no failure (FilesystemUtil.md#permissions_refusal).

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
