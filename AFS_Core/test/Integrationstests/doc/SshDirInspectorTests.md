# SshDirInspectorTests

Module comment for SshDirInspectorTests.cpp.

SshDirInspector feeds the SSH directory dialog of the GUI (SshDirDialog) and had no test of its
own; the sync code lists directories through SyncFileSsh instead. Unix only, like the other SSH
suites that prepare their files on the shared disk.

## SshDirInspectorTest

One directory below the run's temp directory with a file, a subdirectory with a file, a file
symlink and a directory symlink - every entry type the inspector distinguishes. The SSH user
reads it, so it is opened up for everyone.

## InspectDirListsEntriesWithTheirTypes

The plain listing: "." and ".." are left out, each entry carries its type flags, the file its
size and time, and a symlink is reported as a symlink rather than as what it points to.

## InspectDirRecursiveReachesNestedFilesButNotLinkedDirectories

The recursive listing names nested entries by their full path and does not descend into a
directory symlink - the same loop guard as in the sync.

## MissingPathAndPlainFileAreReportedNotListed

Both answers go into error_message with the flags telling them apart, so the dialog can say
"does not exist" or "is not a directory" instead of showing an empty list.

## HomeDirIsAnAbsoluteExistingDirectory

The dialog opens at the SSH user's home; sftp_canonicalize_path(".") must give an absolute
path, and on the local test server it must exist as a directory.
