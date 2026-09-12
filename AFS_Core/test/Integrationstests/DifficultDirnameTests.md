# DifficultDirnameTests

Module comment for DifficultDirnameTests.cpp.

## DIFFICULT_DIR_NAMES

The same categories as DifficultFilenameTests, but without a file extension and without an
embedded backslash: this application reads that as a path separator in several places
(DIR_SEPARATOR, win_to_sftp_path, ...), so a backslash *inside a directory name* would test
separator handling instead of name handling - a different subject from this one.

## to_sync_path

Converts a path built natively (UTF-8) into the wstring form SyncItem::source()/target() expect
- the same detour the rest of the application takes (see util::string_to_wstring), not a raw
widening.

## DirsWithDifficultNamesSyncToTwoTargets

SyncDir copies only the files inside a directory, never its subdirectories, so the difficult
name sits in the source and target directory itself here - the way a user would enter it as a
sync path. That exercises exactly the code path SyncDirTests/SyncSshDirTests already cover with
ordinary names, but with the difficult names from DifficultFilenameTests for the directory
instead of for the file.

## parents-must-exist

SyncDir creates only the last level of the target (create_target_dir_if_not_exist calls
fs::create_directory(), not create_directories()), so the parents have to exist already.

## TreeWithDifficultSubdirNamesSyncsToTwoTargets

SyncTree recurses through the directory structure itself - here the difficult names sit as
subdirectories in ONE tree that SyncTree has to discover through fs::directory_iterator and
rebuild in the target.
