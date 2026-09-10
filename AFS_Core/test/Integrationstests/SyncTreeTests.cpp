#include "pch.h"
#include "SyncTree.h"
#include "Synchronize.h"
#include "DirUtil.h"
#include "SyncTreeTests.h"
#include "Config.h"
#include "SshAccount.h"
#include <Logging.h>
#include <filesystem>
#include <fstream>
#include <StrUtil.h>

using namespace AFS_Core;

using namespace SyncTreeTests;
using namespace util;

namespace
{
    void copy_resource_to_test_dir()
    {
        tu::erase_test_dir(COUNT_TEST_SOURCE_PATH, true);
        tu::erase_test_dir(COUNT_TEST_TARGET_PATH, true);

        std::filesystem::create_directories(wstring_to_string(COUNT_TEST_SOURCE_PATH));
        std::filesystem::copy(wstring_to_string(RESOURCE_PATH), 
                              wstring_to_string(COUNT_TEST_SOURCE_PATH),
                              std::filesystem::copy_options::recursive);
    }
}

// Test: Synchronisation eines verschachtelten Verzeichnisbaums
TEST(SyncTreeTests, SyncTreeWithNestedDirectories)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);

    // Erstelle verschachtelte Verzeichnisstruktur
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "subdir1/subdir2");
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "subdir3");

    // Erstelle Dateien in verschiedenen Ebenen
    std::ofstream(wstring_to_string(SOURCE_PATH) + "root_file.txt") << "root content";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "subdir1/sub1_file.txt") << "subdir1 content";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "subdir1/subdir2/sub2_file.txt") << "subdir2 content";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "subdir3/sub3_file.txt") << "subdir3 content";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);

    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    // Prüfe, dass Zielverzeichnis existiert
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH)));

    // Prüfe, dass alle Verzeichnisse kopiert wurden
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/subdir1"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/subdir1/subdir2"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/subdir3"));

    // Prüfe, dass alle Dateien kopiert wurden
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/root_file.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/subdir1/sub1_file.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/subdir1/subdir2/sub2_file.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/subdir3/sub3_file.txt"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Test: Synchronisation eines leeren Verzeichnisbaums
TEST(SyncTreeTests, SyncTreeEmptyStructure)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);

    // Erstelle nur leere Verzeichnisstruktur
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "empty1/empty2/empty3");
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "empty4");

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);

    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    // Prüfe, dass Struktur kopiert wurde
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH)));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/empty1/empty2/empty3"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/empty4"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Test: Synchronisation mit gemischtem Baum (Dateien und Verzeichnisse)
TEST(SyncTreeTests, SyncTreeMixedContent)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);

    // Erstelle gemischte Struktur
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "dir1");
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "dir2/nested");

    std::ofstream(wstring_to_string(SOURCE_PATH) + "file1.txt") << "content1";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "dir1/file2.txt") << "content2";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "dir1/file3.md") << "# Markdown";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "dir2/file4.txt") << "content4";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "dir2/nested/file5.txt") << "content5";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);

    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    // Prüfe alle Dateien
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/file1.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/dir1/file2.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/dir1/file3.md"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/dir2/file4.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/dir2/nested/file5.txt"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Test: Synchronisation mit bestehenden Zieldateien
TEST(SyncTreeTests, SyncTreeWithExistingTarget)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);

    // Erstelle Quellstruktur
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "subdir");
    std::ofstream(wstring_to_string(SOURCE_PATH) + "new_file.txt") << "new";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "subdir/sub_file.txt") << "sub";

    // Erstelle bereits existierende Zielstruktur
    std::filesystem::create_directories(wstring_to_string(TARGET_PATH) + "/old_subdir");
    std::ofstream(wstring_to_string(TARGET_PATH) + "/old_file.txt") << "old";
    std::ofstream(wstring_to_string(TARGET_PATH) + "/old_subdir/old_sub_file.txt") << "old_sub";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);

    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    // Prüfe neue Dateien wurden kopiert
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/new_file.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/subdir/sub_file.txt"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Test: Synchronisation mit vielen Dateien
TEST(SyncTreeTests, SyncTreeWithManyFiles)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);

    // Erstelle Verzeichnis mit vielen Dateien
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "many_files");

    const int file_count = 100;
    for (int i = 0; i < file_count; ++i) {
        std::string filename = wstring_to_string(SOURCE_PATH) + "many_files/file_" + std::to_string(i) + ".txt";
        std::ofstream(filename) << "Content of file " << i;
    }

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);

    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    // Prüfe, dass alle Dateien kopiert wurden
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/many_files"));
    for (int i = 0; i < file_count; ++i) {
        std::string filename = wstring_to_string(TARGET_PATH) + "/many_files/file_" + std::to_string(i) + ".txt";
        EXPECT_TRUE(std::filesystem::exists(filename));
    }

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Test: Synchronisation mit verschiedenen Dateitypen
TEST(SyncTreeTests, SyncTreeWithDifferentFileTypes)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);

    // Erstelle Dateien mit verschiedenen Endungen
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH));

    std::ofstream(wstring_to_string(SOURCE_PATH) + "document.txt") << "Text file";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "script.sh") << "#!/bin/bash\necho 'test'";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "config.json") << "{\"key\": \"value\"}";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "data.xml") << "<?xml version=\"1.0\"?><root/>";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "README.md") << "# README";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "image.bin") << std::string(1024, '\0');

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);

    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    // Prüfe alle verschiedenen Dateitypen
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/document.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/script.sh"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/config.json"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/data.xml"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/README.md"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/image.bin"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Test: Synchronisation mit tiefer Verzeichnishierarchie
TEST(SyncTreeTests, SyncTreeWithDeepHierarchy)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);

    // Erstelle sehr tiefe Verzeichnisstruktur
    std::string deep_path = wstring_to_string(SOURCE_PATH);
    for (int i = 0; i < 10; ++i) {
        deep_path += "level" + std::to_string(i) + "/";
    }
    std::filesystem::create_directories(deep_path);
    std::ofstream(deep_path + "deep_file.txt") << "Deep content";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);

    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    // Prüfe, dass die tiefe Struktur kopiert wurde
    std::string target_deep_path = wstring_to_string(TARGET_PATH);
    for (int i = 0; i < 10; ++i) {
        target_deep_path += "/level" + std::to_string(i);
    }
    EXPECT_TRUE(std::filesystem::exists(target_deep_path + "/deep_file.txt"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Test: Synchronisation mit Dateien mit speziellen Zeichen im Namen
TEST(SyncTreeTests, SyncTreeWithSpecialCharacters)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);

    // Erstelle Dateien mit speziellen Zeichen (Unix-kompatibel)
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH));

    std::ofstream(wstring_to_string(SOURCE_PATH) + "file_with_spaces.txt") << "spaces";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "file-with-dashes.txt") << "dashes";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "file_with_underscores.txt") << "underscores";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "file.multiple.dots.txt") << "dots";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);

    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    // Prüfe alle Dateien mit speziellen Zeichen
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/file_with_spaces.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/file-with-dashes.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/file_with_underscores.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/file.multiple.dots.txt"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Test: Synchronisation mit großer Datei
TEST(SyncTreeTests, SyncTreeWithLargeFile)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);

    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH));

    // Erstelle eine größere Datei (1 MB)
    std::ofstream large_file(wstring_to_string(SOURCE_PATH) + "large_file.dat", std::ios::binary);
    std::string chunk(1024, 'X');
    for (int i = 0; i < 1024; ++i) {
        large_file << chunk;
    }
    large_file.close();

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);

    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    // Prüfe, dass die große Datei kopiert wurde
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/large_file.dat"));

    // Prüfe Dateigröße
    auto source_size = std::filesystem::file_size(wstring_to_string(SOURCE_PATH) + "large_file.dat");
    auto target_size = std::filesystem::file_size(wstring_to_string(TARGET_PATH) + "/large_file.dat");
    EXPECT_EQ(source_size, target_size);

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Test: Synchronisation mit komplexer gemischter Struktur
TEST(SyncTreeTests, SyncTreeComplexStructure)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);

    // Erstelle komplexe Struktur mit mehreren Ebenen
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "project/src/main");
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "project/src/utils");
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "project/tests");
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "project/docs");
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "project/build");

    // Dateien in verschiedenen Verzeichnissen
    std::ofstream(wstring_to_string(SOURCE_PATH) + "project/README.md") << "# Project";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "project/src/main/main.cpp") << "int main() {}";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "project/src/main/config.h") << "#pragma once";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "project/src/utils/helper.cpp") << "// helper";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "project/src/utils/helper.h") << "// helper.h";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "project/tests/test_main.cpp") << "// test";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "project/docs/manual.txt") << "Manual";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);

    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    // Prüfe komplette Struktur
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/project/README.md"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/project/src/main/main.cpp"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/project/src/main/config.h"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/project/src/utils/helper.cpp"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/project/src/utils/helper.h"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/project/tests/test_main.cpp"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/project/docs/manual.txt"));
    EXPECT_TRUE(std::filesystem::is_directory(wstring_to_string(TARGET_PATH) + "/project/build"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Test: Nur Dateien zählen ohne Kopieren (CountFiles)
TEST(SyncTreeTests, SyncTreeCountFilesOnly)
{
    copy_resource_to_test_dir();

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(COUNT_TEST_SOURCE_PATH);
    si.target(COUNT_TEST_TARGET_PATH);

    std::size_t copy_count = 0;
    // Letzter Parameter 'true' bedeutet: nur zählen, nicht kopieren
    SyncTree test(si, copy_count, true);
    test();

    // Prüfe, dass genau 4 Dateien gezählt wurden
    EXPECT_EQ(copy_count, 4);

    // Prüfe, dass KEINE Dateien tatsächlich kopiert wurden
    EXPECT_FALSE(std::filesystem::exists(wstring_to_string(COUNT_TEST_TARGET_PATH)));

    tu::erase_test_dir(COUNT_TEST_SOURCE_PATH, true);
    tu::erase_test_dir(COUNT_TEST_TARGET_PATH, true);
}

// Regression test for Fehler 3: SyncTree::sync_branch() recursed via
// fs::directory_entry::is_directory(), which follows symlinks by default,
// with no cycle detection. On Linux the kernel's own ELOOP guard eventually
// aborts a self-referencing symlink chain (confirmed: ~40 levels, ~80 wasted
// directories physically created in the target) before the process crashes
// from stack exhaustion - so a plain "does it crash" check is not reliable
// across platforms/filesystems. The deterministic, portable assertion is
// that a symlinked directory must never be descended into at all.
TEST(SyncTreeTests, SyncTreeSymlinkCycleDoesNotCrash)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);

    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "subdir");
    std::ofstream(wstring_to_string(SOURCE_PATH) + "file.txt") << "content";
    const std::string loop_path = wstring_to_string(SOURCE_PATH) + "subdir/loop";
    // Symlink inside the tree pointing back to the tree's own root - a cycle.
    try {
        std::filesystem::create_directory_symlink(wstring_to_string(SOURCE_PATH), loop_path);
    }
    catch (const std::filesystem::filesystem_error& e) {
        // Windows verlangt fuer Symlinks SeCreateSymbolicLinkPrivilege, also
        // Entwicklermodus oder eine erhoehte Sitzung. Fehlt beides, laesst sich
        // die Vorbedingung nicht herstellen - ueberspringen statt scheitern.
        tu::erase_test_dir(SOURCE_PATH, true);
        GTEST_SKIP() << "Symlink nicht anlegbar (fehlende Berechtigung): " << e.what();
    }

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);

    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test(); // must return promptly instead of recursing through the symlink

    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/file.txt"));
    EXPECT_FALSE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/subdir/loop"))
        << "a symlinked directory must never be followed/recreated - that's what let this recurse in the first place";

    std::filesystem::remove(loop_path); // drop the symlink itself before recursive cleanup
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Fehler 1 (round 7): like SyncDir, SyncTree only ever enumerated the source
// side at every recursion level, so delete_in_target/two_way_sync had no code
// path for Tree items either. Verifies both the top level and a *nested*
// subdirectory level correctly detect a target-only file (proving the fix
// applies per recursion level, not just once at the top).
TEST(SyncTreeTests, DeleteInTargetRemovesFileMissingFromSourceAtEveryLevel)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "subdir");
    std::filesystem::create_directories(wstring_to_string(TARGET_PATH) + "/subdir");
    std::ofstream(wstring_to_string(SOURCE_PATH) + "kept_root.txt") << "kept";
    std::ofstream(wstring_to_string(TARGET_PATH) + "/kept_root.txt") << "kept";
    std::ofstream(wstring_to_string(TARGET_PATH) + "/orphan_root.txt") << "gone from source";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "subdir/kept_nested.txt") << "kept";
    std::ofstream(wstring_to_string(TARGET_PATH) + "/subdir/kept_nested.txt") << "kept";
    std::ofstream(wstring_to_string(TARGET_PATH) + "/subdir/orphan_nested.txt") << "gone from source";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    si.delete_in_target(true);
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    EXPECT_FALSE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/orphan_root.txt"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/kept_root.txt"));
    EXPECT_FALSE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/subdir/orphan_nested.txt"))
        << "the target-only pass must run at every recursion level, not just the top";
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/subdir/kept_nested.txt"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

TEST(SyncTreeTests, TwoWaySyncCopiesTargetOnlyFileBackToSourceAtNestedLevel)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "subdir");
    std::filesystem::create_directories(wstring_to_string(TARGET_PATH) + "/subdir");
    std::ofstream(wstring_to_string(TARGET_PATH) + "/subdir/newintarget.txt") << "bring me back";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    si.two_way_sync(true);
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    ASSERT_TRUE(std::filesystem::exists(wstring_to_string(SOURCE_PATH) + "subdir/newintarget.txt"));
    std::ifstream in(wstring_to_string(SOURCE_PATH) + "subdir/newintarget.txt");
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "bring me back");

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// Exclusions feature: an excluded *directory* must be skipped entirely (never
// even entered), not just filtered file-by-file - the whole point of
// excluding e.g. "node_modules" or ".git" as a subtree, matching how the
// symlink-loop protection above already treats directories specially rather
// than relying on per-file filtering.
TEST(SyncTreeTests, ExcludedDirectoryIsNeverEnteredNotJustFilteredFileByFile)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "node_modules");
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "src");
    std::ofstream(wstring_to_string(SOURCE_PATH) + "node_modules/pkg.js") << "dependency";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "src/main.cpp") << "source code";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    Exclusions exclusions;
    exclusions.push_back(L"node_modules");
    si.exclusions(exclusions);
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    EXPECT_FALSE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/node_modules"))
        << "the excluded directory itself must not even be created in the target";
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/src/main.cpp"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

TEST(SyncTreeTests, ExcludedFileIsSkippedAtEveryNestedLevel)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(wstring_to_string(SOURCE_PATH) + "subdir");
    std::ofstream(wstring_to_string(SOURCE_PATH) + "debug.log") << "root log";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "subdir/debug.log") << "nested log";
    std::ofstream(wstring_to_string(SOURCE_PATH) + "subdir/keep.txt") << "keep";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    Exclusions exclusions;
    exclusions.push_back(L"*.log");
    si.exclusions(exclusions);
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    EXPECT_FALSE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/debug.log"));
    EXPECT_FALSE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/subdir/debug.log"));
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/subdir/keep.txt"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
}

// See SyncDirTests.DeleteInTargetKeepsTargetWhenSourceDirIsGone - for a tree
// the stakes are higher still, since a whole backup hierarchy hangs off the
// target. An unavailable source root must leave every level untouched.
TEST(SyncTreeTests, DeleteInTargetKeepsTargetWhenSourceDirIsGone)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(wstring_to_string(TARGET_PATH) + "/subdir");
    std::ofstream(wstring_to_string(TARGET_PATH) + "/backup_root.txt") << "irreplaceable";
    std::ofstream(wstring_to_string(TARGET_PATH) + "/subdir/backup_nested.txt") << "irreplaceable";
    ASSERT_FALSE(std::filesystem::exists(SOURCE_PATH));

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    si.delete_in_target(true);
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    EXPECT_NO_THROW(test());

    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/backup_root.txt"))
        << "an unavailable source directory must never be read as 'everything was deleted'";
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/subdir/backup_nested.txt"));
    EXPECT_EQ(copy_count, 0u);

    tu::erase_test_dir(TARGET_PATH, true);
}

// A source subdirectory that disappears while the source root still exists:
// only that branch's target counterpart may be affected, and since SyncTree
// never descends into a target-only directory, its files stay. Pins down the
// current, deliberate boundary of the option - deletions are mirrored per
// file, not for whole directories.
TEST(SyncTreeTests, DeleteInTargetLeavesTargetOnlyDirectoriesAlone)
{
    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(SOURCE_PATH);
    std::filesystem::create_directories(wstring_to_string(TARGET_PATH) + "/gone_from_source");
    std::ofstream(wstring_to_string(SOURCE_PATH) + "kept.txt") << "kept";
    std::ofstream(wstring_to_string(TARGET_PATH) + "/gone_from_source/orphan.txt") << "orphaned";

    Synchronize::start();
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(SOURCE_PATH);
    si.target(TARGET_PATH);
    si.delete_in_target(true);
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    test();

    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/gone_from_source/orphan.txt"))
        << "target-only directories are not traversed, so their contents survive - "
           "a directory removed in the source is not mirrored as a deletion";
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/kept.txt"));

    tu::erase_test_dir(SOURCE_PATH, true);
    tu::erase_test_dir(TARGET_PATH, true);
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
        account.set_name("unreachable");
        account.set_user("nobody");
        account.set_host("this-host-does-not-exist.invalid");
        account.set_password("irrelevant");
        Config::instance().add(account);
        Synchronize::start();
    }

    void TearDown() override
    {
        Config::instance().reset();
        tu::erase_test_dir(TARGET_PATH, true);
    }
};

} // namespace

// ssh_list_dir() cannot distinguish "connection failed" from "empty directory",
// so the target-only pass runs over the whole target at every recursion level.
// FileDifference's per-file check is what keeps the files - assert it holds
// for the nested level too, where a tree has most of its data.
TEST_F(SyncTreeUnreachableSshSourceTest, DeleteInTargetKeepsTargetWhenSshSourceIsUnreachable)
{
    tu::erase_test_dir(TARGET_PATH, true);
    std::filesystem::create_directories(wstring_to_string(TARGET_PATH) + "/subdir");
    std::ofstream(wstring_to_string(TARGET_PATH) + "/backup_root.txt") << "irreplaceable";
    std::ofstream(wstring_to_string(TARGET_PATH) + "/subdir/backup_nested.txt") << "irreplaceable";

    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Tree);
    si.source(L"unreachable:/remote/backup");
    si.target(TARGET_PATH);
    si.delete_in_target(true);
    ASSERT_TRUE(si.ssh_source());
    std::size_t copy_count = 0;
    SyncTree test(si, copy_count, false);
    EXPECT_NO_THROW(test());

    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/backup_root.txt"))
        << "an unreachable SSH source lists as empty - the target must not be emptied along with it";
    EXPECT_TRUE(std::filesystem::exists(wstring_to_string(TARGET_PATH) + "/subdir/backup_nested.txt"));
}
