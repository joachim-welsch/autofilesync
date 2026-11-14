# CloudFoldersTests

Module comment for CloudFoldersTests.cpp.

## CloudFoldersTest

Every test gets a home directory of its own below the temp directory and points the variables
the detection reads at it (HOME on Unix, APPDATA and LOCALAPPDATA on Windows, where the
OneDrive variables are unset as well), so the cloud clients installed on the test machine play
no part and nothing outside the temp directory is written. TearDown restores the variables
and removes the directory.

## folders_of

The folders of one provider family, matched by the start of the provider name so "Dropbox"
covers "Dropbox Business" too. The tests compare per provider instead of the whole result
because on Windows the Google Drive lookup asks the real drives and cannot be redirected.

## NextcloudFoldersOfAllGroups

The file has the shape the Windows client writes: CR LF line ends, a quoted value, a trailing
slash and keys of other kinds between the folder keys.
