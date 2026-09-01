#include "pch.h"
#include "SyncTree.h"
#include "Synchronize.h"
#include "DirUtil.h"
#include "SyncTreeTests.h"
#include "config.h"
#include "SshAccount.h"
#include <Logging.h>
#include <filesystem>
#include <fstream>
#include <strutil.h>

using namespace SyncTreeTests;
using namespace util;

namespace
{
    void copyResourceToTestDir()
    {
        tu::eraseTestDir(CountTestSourcePath, true);
        tu::eraseTestDir(CountTestTargetPath, true);

        std::filesystem::create_directories(wstring_to_string(CountTestSourcePath));
        std::filesystem::copy(wstring_to_string(ResourcePath), 
                              wstring_to_string(CountTestSourcePath),
                              std::filesystem::copy_options::recursive);
    }
}

// Test: Synchronisation eines verschachtelten Verzeichnisbaums
TEST(SyncTreeTests, SyncTreeWithNestedDirectories)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);

    // Erstelle verschachtelte Verzeichnisstruktur
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "subdir1/subdir2");
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "subdir3");

    // Erstelle Dateien in verschiedenen Ebenen
    std::ofstream(wstring_to_string(SourcePath) + "root_file.txt") << "root content";
    std::ofstream(wstring_to_string(SourcePath) + "subdir1/sub1_file.txt") << "subdir1 content";
    std::ofstream(wstring_to_string(SourcePath) + "subdir1/subdir2/sub2_file.txt") << "subdir2 content";
    std::ofstream(wstring_to_string(SourcePath) + "subdir3/sub3_file.txt") << "subdir3 content";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);

    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    // Prüfe, dass Zielverzeichnis existiert
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath)));

    // Prüfe, dass alle Verzeichnisse kopiert wurden
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/subdir1"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/subdir1/subdir2"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/subdir3"));

    // Prüfe, dass alle Dateien kopiert wurden
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/root_file.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/subdir1/sub1_file.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/subdir1/subdir2/sub2_file.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/subdir3/sub3_file.txt"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Test: Synchronisation eines leeren Verzeichnisbaums
TEST(SyncTreeTests, SyncTreeEmptyStructure)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);

    // Erstelle nur leere Verzeichnisstruktur
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "empty1/empty2/empty3");
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "empty4");

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);

    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    // Prüfe, dass Struktur kopiert wurde
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath)));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/empty1/empty2/empty3"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/empty4"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Test: Synchronisation mit gemischtem Baum (Dateien und Verzeichnisse)
TEST(SyncTreeTests, SyncTreeMixedContent)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);

    // Erstelle gemischte Struktur
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "dir1");
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "dir2/nested");

    std::ofstream(wstring_to_string(SourcePath) + "file1.txt") << "content1";
    std::ofstream(wstring_to_string(SourcePath) + "dir1/file2.txt") << "content2";
    std::ofstream(wstring_to_string(SourcePath) + "dir1/file3.md") << "# Markdown";
    std::ofstream(wstring_to_string(SourcePath) + "dir2/file4.txt") << "content4";
    std::ofstream(wstring_to_string(SourcePath) + "dir2/nested/file5.txt") << "content5";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);

    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    // Prüfe alle Dateien
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/file1.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/dir1/file2.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/dir1/file3.md"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/dir2/file4.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/dir2/nested/file5.txt"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Test: Synchronisation mit bestehenden Zieldateien
TEST(SyncTreeTests, SyncTreeWithExistingTarget)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);

    // Erstelle Quellstruktur
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "subdir");
    std::ofstream(wstring_to_string(SourcePath) + "new_file.txt") << "new";
    std::ofstream(wstring_to_string(SourcePath) + "subdir/sub_file.txt") << "sub";

    // Erstelle bereits existierende Zielstruktur
    std::filesystem::create_directories(wstring_to_string(TargetPath) + "/old_subdir");
    std::ofstream(wstring_to_string(TargetPath) + "/old_file.txt") << "old";
    std::ofstream(wstring_to_string(TargetPath) + "/old_subdir/old_sub_file.txt") << "old_sub";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);

    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    // Prüfe neue Dateien wurden kopiert
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/new_file.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/subdir/sub_file.txt"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Test: Synchronisation mit vielen Dateien
TEST(SyncTreeTests, SyncTreeWithManyFiles)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);

    // Erstelle Verzeichnis mit vielen Dateien
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "many_files");

    const int fileCount = 100;
    for (int i = 0; i < fileCount; ++i) {
        std::string filename = wstring_to_string(SourcePath) + "many_files/file_" + std::to_string(i) + ".txt";
        std::ofstream(filename) << "Content of file " << i;
    }

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);

    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    // Prüfe, dass alle Dateien kopiert wurden
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/many_files"));
    for (int i = 0; i < fileCount; ++i) {
        std::string filename = wstring_to_string(TargetPath) + "/many_files/file_" + std::to_string(i) + ".txt";
        EXPECT_TRUE(std::filesystem::exists(filename));
    }

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Test: Synchronisation mit verschiedenen Dateitypen
TEST(SyncTreeTests, SyncTreeWithDifferentFileTypes)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);

    // Erstelle Dateien mit verschiedenen Endungen
    std::filesystem::create_directories(wstring_to_string(SourcePath));

    std::ofstream(wstring_to_string(SourcePath) + "document.txt") << "Text file";
    std::ofstream(wstring_to_string(SourcePath) + "script.sh") << "#!/bin/bash\necho 'test'";
    std::ofstream(wstring_to_string(SourcePath) + "config.json") << "{\"key\": \"value\"}";
    std::ofstream(wstring_to_string(SourcePath) + "data.xml") << "<?xml version=\"1.0\"?><root/>";
    std::ofstream(wstring_to_string(SourcePath) + "README.md") << "# README";
    std::ofstream(wstring_to_string(SourcePath) + "image.bin") << std::string(1024, '\0');

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);

    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    // Prüfe alle verschiedenen Dateitypen
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/document.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/script.sh"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/config.json"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/data.xml"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/README.md"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/image.bin"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Test: Synchronisation mit tiefer Verzeichnishierarchie
TEST(SyncTreeTests, SyncTreeWithDeepHierarchy)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);

    // Erstelle sehr tiefe Verzeichnisstruktur
    std::string deepPath = wstring_to_string(SourcePath);
    for (int i = 0; i < 10; ++i) {
        deepPath += "level" + std::to_string(i) + "/";
    }
    std::filesystem::create_directories(deepPath);
    std::ofstream(deepPath + "deep_file.txt") << "Deep content";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);

    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    // Prüfe, dass die tiefe Struktur kopiert wurde
    std::string targetDeepPath = wstring_to_string(TargetPath);
    for (int i = 0; i < 10; ++i) {
        targetDeepPath += "/level" + std::to_string(i);
    }
    EXPECT_TRUE(std::filesystem::exists(targetDeepPath + "/deep_file.txt"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Test: Synchronisation mit Dateien mit speziellen Zeichen im Namen
TEST(SyncTreeTests, SyncTreeWithSpecialCharacters)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);

    // Erstelle Dateien mit speziellen Zeichen (Unix-kompatibel)
    std::filesystem::create_directories(wstring_to_string(SourcePath));

    std::ofstream(wstring_to_string(SourcePath) + "file_with_spaces.txt") << "spaces";
    std::ofstream(wstring_to_string(SourcePath) + "file-with-dashes.txt") << "dashes";
    std::ofstream(wstring_to_string(SourcePath) + "file_with_underscores.txt") << "underscores";
    std::ofstream(wstring_to_string(SourcePath) + "file.multiple.dots.txt") << "dots";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);

    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    // Prüfe alle Dateien mit speziellen Zeichen
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/file_with_spaces.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/file-with-dashes.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/file_with_underscores.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/file.multiple.dots.txt"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Test: Synchronisation mit großer Datei
TEST(SyncTreeTests, SyncTreeWithLargeFile)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);

    std::filesystem::create_directories(wstring_to_string(SourcePath));

    // Erstelle eine größere Datei (1 MB)
    std::ofstream largeFile(wstring_to_string(SourcePath) + "large_file.dat", std::ios::binary);
    std::string chunk(1024, 'X');
    for (int i = 0; i < 1024; ++i) {
        largeFile << chunk;
    }
    largeFile.close();

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);

    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    // Prüfe, dass die große Datei kopiert wurde
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/large_file.dat"));

    // Prüfe Dateigröße
    auto sourceSize = std::filesystem::file_size(wstring_to_string(SourcePath) + "large_file.dat");
    auto targetSize = std::filesystem::file_size(wstring_to_string(TargetPath) + "/large_file.dat");
    EXPECT_EQ(sourceSize, targetSize);

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Test: Synchronisation mit komplexer gemischter Struktur
TEST(SyncTreeTests, SyncTreeComplexStructure)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);

    // Erstelle komplexe Struktur mit mehreren Ebenen
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "project/src/main");
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "project/src/utils");
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "project/tests");
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "project/docs");
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "project/build");

    // Dateien in verschiedenen Verzeichnissen
    std::ofstream(wstring_to_string(SourcePath) + "project/README.md") << "# Project";
    std::ofstream(wstring_to_string(SourcePath) + "project/src/main/main.cpp") << "int main() {}";
    std::ofstream(wstring_to_string(SourcePath) + "project/src/main/config.h") << "#pragma once";
    std::ofstream(wstring_to_string(SourcePath) + "project/src/utils/helper.cpp") << "// helper";
    std::ofstream(wstring_to_string(SourcePath) + "project/src/utils/helper.h") << "// helper.h";
    std::ofstream(wstring_to_string(SourcePath) + "project/tests/test_main.cpp") << "// test";
    std::ofstream(wstring_to_string(SourcePath) + "project/docs/manual.txt") << "Manual";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);

    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    // Prüfe komplette Struktur
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/project/README.md"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/project/src/main/main.cpp"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/project/src/main/config.h"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/project/src/utils/helper.cpp"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/project/src/utils/helper.h"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/project/tests/test_main.cpp"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/project/docs/manual.txt"));
    EXPECT_TRUE(std::filesystem::is_directory(wstring_to_string(TargetPath) + "/project/build"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Test: Nur Dateien zählen ohne Kopieren (CountFiles)
TEST(SyncTreeTests, SyncTreeCountFilesOnly)
{
    copyResourceToTestDir();

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(CountTestSourcePath);
    si.target(CountTestTargetPath);

    std::size_t copyCount = 0;
    // Letzter Parameter 'true' bedeutet: nur zählen, nicht kopieren
    SyncTree test(si, copyCount, true);
    test();

    // Prüfe, dass genau 4 Dateien gezählt wurden
    EXPECT_EQ(copyCount, 4);

    // Prüfe, dass KEINE Dateien tatsächlich kopiert wurden
    EXPECT_FALSE(std::filesystem::exists(wstring_to_string(CountTestTargetPath)));

    tu::eraseTestDir(CountTestSourcePath, true);
    tu::eraseTestDir(CountTestTargetPath, true);
}

// Regression test for Fehler 3: SyncTree::syncBranch() recursed via
// fs::directory_entry::is_directory(), which follows symlinks by default,
// with no cycle detection. On Linux the kernel's own ELOOP guard eventually
// aborts a self-referencing symlink chain (confirmed: ~40 levels, ~80 wasted
// directories physically created in the target) before the process crashes
// from stack exhaustion - so a plain "does it crash" check is not reliable
// across platforms/filesystems. The deterministic, portable assertion is
// that a symlinked directory must never be descended into at all.
TEST(SyncTreeTests, SyncTreeSymlinkCycleDoesNotCrash)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);

    std::filesystem::create_directories(wstring_to_string(SourcePath) + "subdir");
    std::ofstream(wstring_to_string(SourcePath) + "file.txt") << "content";
    const std::string loopPath = wstring_to_string(SourcePath) + "subdir/loop";
    // Symlink inside the tree pointing back to the tree's own root - a cycle.
    try {
        std::filesystem::create_directory_symlink(wstring_to_string(SourcePath), loopPath);
    }
    catch (const std::filesystem::filesystem_error& e) {
        // Windows verlangt fuer Symlinks SeCreateSymbolicLinkPrivilege, also
        // Entwicklermodus oder eine erhoehte Sitzung. Fehlt beides, laesst sich
        // die Vorbedingung nicht herstellen - ueberspringen statt scheitern.
        tu::eraseTestDir(SourcePath, true);
        GTEST_SKIP() << "Symlink nicht anlegbar (fehlende Berechtigung): " << e.what();
    }

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);

    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test(); // must return promptly instead of recursing through the symlink

    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/file.txt"));
    EXPECT_FALSE(std::filesystem::exists(wstring_to_string(TargetPath) + "/subdir/loop"))
        << "a symlinked directory must never be followed/recreated - that's what let this recurse in the first place";

    std::filesystem::remove(loopPath); // drop the symlink itself before recursive cleanup
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Fehler 1 (round 7): like SyncDir, SyncTree only ever enumerated the source
// side at every recursion level, so deleteInTarget/twoWaySync had no code
// path for Tree items either. Verifies both the top level and a *nested*
// subdirectory level correctly detect a target-only file (proving the fix
// applies per recursion level, not just once at the top).
TEST(SyncTreeTests, DeleteInTargetRemovesFileMissingFromSourceAtEveryLevel)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "subdir");
    std::filesystem::create_directories(wstring_to_string(TargetPath) + "/subdir");
    std::ofstream(wstring_to_string(SourcePath) + "kept_root.txt") << "kept";
    std::ofstream(wstring_to_string(TargetPath) + "/kept_root.txt") << "kept";
    std::ofstream(wstring_to_string(TargetPath) + "/orphan_root.txt") << "gone from source";
    std::ofstream(wstring_to_string(SourcePath) + "subdir/kept_nested.txt") << "kept";
    std::ofstream(wstring_to_string(TargetPath) + "/subdir/kept_nested.txt") << "kept";
    std::ofstream(wstring_to_string(TargetPath) + "/subdir/orphan_nested.txt") << "gone from source";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);
    si.deleteInTarget(true);
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    EXPECT_FALSE(std::filesystem::exists(wstring_to_string(TargetPath) + "/orphan_root.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/kept_root.txt"));
    EXPECT_FALSE(std::filesystem::exists(wstring_to_string(TargetPath) + "/subdir/orphan_nested.txt"))
        << "the target-only pass must run at every recursion level, not just the top";
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/subdir/kept_nested.txt"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

TEST(SyncTreeTests, TwoWaySyncCopiesTargetOnlyFileBackToSourceAtNestedLevel)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "subdir");
    std::filesystem::create_directories(wstring_to_string(TargetPath) + "/subdir");
    std::ofstream(wstring_to_string(TargetPath) + "/subdir/newintarget.txt") << "bring me back";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);
    si.twoWaySync(true);
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    ASSERT_TRUE(std::filesystem::exists(wstring_to_string(SourcePath) + "subdir/newintarget.txt"));
    std::ifstream in(wstring_to_string(SourcePath) + "subdir/newintarget.txt");
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "bring me back");

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// Exclusions feature: an excluded *directory* must be skipped entirely (never
// even entered), not just filtered file-by-file - the whole point of
// excluding e.g. "node_modules" or ".git" as a subtree, matching how the
// symlink-loop protection above already treats directories specially rather
// than relying on per-file filtering.
TEST(SyncTreeTests, ExcludedDirectoryIsNeverEnteredNotJustFilteredFileByFile)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "node_modules");
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "src");
    std::ofstream(wstring_to_string(SourcePath) + "node_modules/pkg.js") << "dependency";
    std::ofstream(wstring_to_string(SourcePath) + "src/main.cpp") << "source code";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);
    Exclusions exclusions;
    exclusions.push_back(L"node_modules");
    si.exclusions(exclusions);
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    EXPECT_FALSE(std::filesystem::exists(wstring_to_string(TargetPath) + "/node_modules"))
        << "the excluded directory itself must not even be created in the target";
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/src/main.cpp"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

TEST(SyncTreeTests, ExcludedFileIsSkippedAtEveryNestedLevel)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(wstring_to_string(SourcePath) + "subdir");
    std::ofstream(wstring_to_string(SourcePath) + "debug.log") << "root log";
    std::ofstream(wstring_to_string(SourcePath) + "subdir/debug.log") << "nested log";
    std::ofstream(wstring_to_string(SourcePath) + "subdir/keep.txt") << "keep";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);
    Exclusions exclusions;
    exclusions.push_back(L"*.log");
    si.exclusions(exclusions);
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    EXPECT_FALSE(std::filesystem::exists(wstring_to_string(TargetPath) + "/debug.log"));
    EXPECT_FALSE(std::filesystem::exists(wstring_to_string(TargetPath) + "/subdir/debug.log"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/subdir/keep.txt"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

// See SyncDirTests.DeleteInTargetKeepsTargetWhenSourceDirIsGone - for a tree
// the stakes are higher still, since a whole backup hierarchy hangs off the
// target. An unavailable source root must leave every level untouched.
TEST(SyncTreeTests, DeleteInTargetKeepsTargetWhenSourceDirIsGone)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(wstring_to_string(TargetPath) + "/subdir");
    std::ofstream(wstring_to_string(TargetPath) + "/backup_root.txt") << "irreplaceable";
    std::ofstream(wstring_to_string(TargetPath) + "/subdir/backup_nested.txt") << "irreplaceable";
    ASSERT_FALSE(std::filesystem::exists(SourcePath));

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);
    si.deleteInTarget(true);
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    EXPECT_NO_THROW(test());

    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/backup_root.txt"))
        << "an unavailable source directory must never be read as 'everything was deleted'";
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/subdir/backup_nested.txt"));
    EXPECT_EQ(copyCount, 0u);

    tu::eraseTestDir(TargetPath, true);
}

// A source subdirectory that disappears while the source root still exists:
// only that branch's target counterpart may be affected, and since SyncTree
// never descends into a target-only directory, its files stay. Pins down the
// current, deliberate boundary of the option - deletions are mirrored per
// file, not for whole directories.
TEST(SyncTreeTests, DeleteInTargetLeavesTargetOnlyDirectoriesAlone)
{
    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(SourcePath);
    std::filesystem::create_directories(wstring_to_string(TargetPath) + "/gone_from_source");
    std::ofstream(wstring_to_string(SourcePath) + "kept.txt") << "kept";
    std::ofstream(wstring_to_string(TargetPath) + "/gone_from_source/orphan.txt") << "orphaned";

    Synchronize::start();
    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(SourcePath);
    si.target(TargetPath);
    si.deleteInTarget(true);
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    test();

    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/gone_from_source/orphan.txt"))
        << "target-only directories are not traversed, so their contents survive - "
           "a directory removed in the source is not mirrored as a deletion";
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/kept.txt"));

    tu::eraseTestDir(SourcePath, true);
    tu::eraseTestDir(TargetPath, true);
}

namespace {

// See the identical fixture in SyncDirTests.cpp / FileDifferenceTests.cpp:
// ".invalid" can never resolve, so every SSH call fails fast without needing
// a real server.
class SyncTreeUnreachableSshSourceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        Config::instance().reset();
        SshAccount account;
        account.setName("unreachable");
        account.setUser("nobody");
        account.setHost("this-host-does-not-exist.invalid");
        account.setPassword("irrelevant");
        Config::instance().add(account);
        Synchronize::start();
    }

    void TearDown() override
    {
        Config::instance().reset();
        tu::eraseTestDir(TargetPath, true);
    }
};

} // namespace

// sshListDir() cannot distinguish "connection failed" from "empty directory",
// so the target-only pass runs over the whole target at every recursion level.
// FileDifference's per-file check is what keeps the files - assert it holds
// for the nested level too, where a tree has most of its data.
TEST_F(SyncTreeUnreachableSshSourceTest, DeleteInTargetKeepsTargetWhenSshSourceIsUnreachable)
{
    tu::eraseTestDir(TargetPath, true);
    std::filesystem::create_directories(wstring_to_string(TargetPath) + "/subdir");
    std::ofstream(wstring_to_string(TargetPath) + "/backup_root.txt") << "irreplaceable";
    std::ofstream(wstring_to_string(TargetPath) + "/subdir/backup_nested.txt") << "irreplaceable";

    SyncItem si;
    si.syncCycle(0);
    si.syncItemType(SyncItemType::Tree);
    si.source(L"unreachable:/remote/backup");
    si.target(TargetPath);
    si.deleteInTarget(true);
    ASSERT_TRUE(si.sshSource());
    std::size_t copyCount = 0;
    SyncTree test(si, copyCount, false);
    EXPECT_NO_THROW(test());

    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/backup_root.txt"))
        << "an unreachable SSH source lists as empty - the target must not be emptied along with it";
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TargetPath) + "/subdir/backup_nested.txt"));
}
