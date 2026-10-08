# DifficultDirnameTests

Module comment for DifficultDirnameTests.cpp.

## DIFFICULT_DIR_NAMES

The same categories as DifficultFilenameTests, but without a file extension and without an
embedded backslash: this application reads that as a path separator in several places
(DIR_SEPARATOR, win_to_sftp_path, ...), so a backslash *inside a directory name* would test
separator handling instead of name handling - a different subject from this one.

## windows-names

Same restriction as DifficultFilenameTests.md#windows-names: Win32 forbids `<>?*` and line
breaks and strips trailing spaces and dots from directory names, so only the representable
variants remain. The 150 characters of the long name already fit below MAX_PATH together with
the temp directory and "/datei.txt".

## utf8-paths

Same reason as DifficultFilenameTests.md#utf8-paths: fs::path(std::string) reads the ANSI code
page on Windows, util::string_to_wstring reads UTF-8 - the test would create one name and sync
another.

## to_sync_path

Converts a path into the wstring form SyncItem::source()/target() expect via its UTF-8 form - the
same detour the rest of the application takes (see util::string_to_wstring), not a raw widening.

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
