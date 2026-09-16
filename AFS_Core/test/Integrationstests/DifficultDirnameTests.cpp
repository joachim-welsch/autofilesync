#include "pch.h"
#include "SyncDir.h"
#include "SyncTree.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "Util.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace AFS_Core;

namespace fs = std::filesystem;
using std::wstring;
using std::string;

namespace {
    const wstring BASE_DIR = TestPaths::scratch_dir() + L"/difficult_dirnames";

    const std::vector<string> DIFFICULT_DIR_NAMES = {  // [DifficultDirnameTests.md#DIFFICULT_DIR_NAMES]
        string(150, 'b'),
        "Verzeichnis mit Leerzeichen",
        ".verstecktes_verzeichnis",
        "Ünïcödé_日本語_😀_Verzeichnis",
        "-rf",
#ifdef _UNIX
        "   mehrfache   Leerzeichen   ",
        "verzeichnis_mit_leerzeichen_am_ende ",
        "special!@#$%^&*()_+-=[]{};'~,<>?",
        "...nur_punkte....",
        "zeilen\numbruch_verzeichnis",
        "abschliessender_punkt.",
#else
        "   mehrfache   Leerzeichen",  // [DifficultDirnameTests.md#windows-names]
        "special!@#$%^&()_+-=[]{};'~,",
        "...nur_punkte",
#endif
    };

    fs::path from_utf8(const string& name)  // [DifficultDirnameTests.md#utf8-paths]
    {
        return fs::path(std::u8string(name.begin(), name.end()));
    }

    wstring to_sync_path(const fs::path& p)  // [DifficultDirnameTests.md#to_sync_path]
    {
        const std::u8string utf8 = p.u8string();
        return util::string_to_wstring(string(utf8.begin(), utf8.end()));
    }
}

TEST(DifficultDirnameTests, DirsWithDifficultNamesSyncToTwoTargets)  // [DifficultDirnameTests.md#DirsWithDifficultNamesSyncToTwoTargets]
{
    const fs::path source_root = fs::path(BASE_DIR) / "sync_dir_quelle";
    const fs::path target1_root = fs::path(BASE_DIR) / "sync_dir_ziel1";
    const fs::path target2_root = fs::path(BASE_DIR) / "sync_dir_ziel2";

    fs::remove_all(source_root);
    fs::remove_all(target1_root);
    fs::remove_all(target2_root);
    fs::create_directories(source_root);
    fs::create_directories(target1_root);  // [DifficultDirnameTests.md#parents-must-exist]
    fs::create_directories(target2_root);

    Synchronize::start();
    for (const auto& name : DIFFICULT_DIR_NAMES) {
        const fs::path source_dir = source_root / from_utf8(name);
        fs::create_directories(source_dir);
        std::ofstream(source_dir / "inhalt.txt") << "inhalt";

        for (const fs::path& target_root : { target1_root, target2_root }) {
            const fs::path target_dir = target_root / from_utf8(name);
            SyncItem si;
            si.sync_cycle(0);
            si.sync_item_type(SyncItemType::Dir);
            si.source(to_sync_path(source_dir));
            si.target(to_sync_path(target_dir));
            std::size_t copy_count = 0;
            SyncDir sync(si, copy_count, false);
            sync();
            EXPECT_EQ(copy_count, 1u) << "Verzeichnisname: " << name;
            EXPECT_TRUE(fs::exists(target_dir / "inhalt.txt")) << "Verzeichnisname: " << name;
        }
    }

    fs::remove_all(source_root);
    fs::remove_all(target1_root);
    fs::remove_all(target2_root);
}

TEST(DifficultDirnameTests, TreeWithDifficultSubdirNamesSyncsToTwoTargets)  // [DifficultDirnameTests.md#TreeWithDifficultSubdirNamesSyncsToTwoTargets]
{
    const fs::path source_root = fs::path(BASE_DIR) / "sync_tree_quelle";
    const fs::path target1_root = fs::path(BASE_DIR) / "sync_tree_ziel1";
    const fs::path target2_root = fs::path(BASE_DIR) / "sync_tree_ziel2";

    fs::remove_all(source_root);
    fs::remove_all(target1_root);
    fs::remove_all(target2_root);
    fs::create_directories(source_root);
    for (const auto& name : DIFFICULT_DIR_NAMES) {
        fs::create_directories(source_root / from_utf8(name));
        std::ofstream(source_root / from_utf8(name) / "datei.txt") << "inhalt";
    }

    Synchronize::start();
    for (const fs::path& target_root : { target1_root, target2_root }) {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(SyncItemType::Tree);
        si.source(to_sync_path(source_root));
        si.target(to_sync_path(target_root));
        std::size_t copy_count = 0;
        SyncTree sync(si, copy_count, false);
        sync();
        EXPECT_EQ(copy_count, DIFFICULT_DIR_NAMES.size());

        for (const auto& name : DIFFICULT_DIR_NAMES)
            EXPECT_TRUE(fs::exists(target_root / from_utf8(name) / "datei.txt")) << "Verzeichnisname: " << name;
    }

    fs::remove_all(source_root);
    fs::remove_all(target1_root);
    fs::remove_all(target2_root);
}
