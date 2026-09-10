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

    // Gleiche Kategorien wie DifficultFilenameTests, aber ohne Dateiendung und
    // ohne eingebetteten Backslash: der wird von dieser App an mehreren
    // Stellen als Pfadtrenner interpretiert (DIR_SEPARATOR, win_to_sftp_path,
    // ...), ein Backslash *im Verzeichnisnamen* wuerde also Trennzeichen- statt
    // Namensverarbeitung testen - ein anderes Thema als hier.
    const std::vector<string> DIFFICULT_DIR_NAMES = {
        string(150, 'b'),
        "Verzeichnis mit Leerzeichen",
        "   mehrfache   Leerzeichen   ",
        "verzeichnis_mit_leerzeichen_am_ende ",
        "special!@#$%^&*()_+-=[]{};'~,<>?",
        ".verstecktes_verzeichnis",
        "...nur_punkte....",
        "Ünïcödé_日本語_😀_Verzeichnis",
        "zeilen\numbruch_verzeichnis",
        "abschliessender_punkt.",
        "-rf",
    };

    // Wandelt einen nativ (UTF-8) aufgebauten Pfad in die wstring-Form um, die
    // SyncItem::source()/target() erwarten - derselbe Umweg wie im Rest der
    // App (siehe util::string_to_wstring), nicht ein rohes Verbreitern.
    wstring to_sync_path(const fs::path& p)
    {
        return util::string_to_wstring(p.string());
    }
}

// SyncDir kopiert nur Dateien innerhalb eines Verzeichnisses, nie
// Unterverzeichnisse - der schwierige Name steckt hier also im Quell- und
// Zielverzeichnis selbst, so wie ein Benutzer ihn als Sync-Pfad eintragen
// wuerde. Das uebt genau den Codepfad, den SyncDirTests/SyncSshDirTests mit
// gewoehnlichen Namen schon abdecken, hier aber mit den schwierigen Namen aus
// DifficultFilenameTests fuer das Verzeichnis statt fuer die Datei.
TEST(DifficultDirnameTests, DirsWithDifficultNamesSyncToTwoTargets)
{
    const fs::path source_root = fs::path(util::wstring_to_string(BASE_DIR)) / "sync_dir_quelle";
    const fs::path target1_root = fs::path(util::wstring_to_string(BASE_DIR)) / "sync_dir_ziel1";
    const fs::path target2_root = fs::path(util::wstring_to_string(BASE_DIR)) / "sync_dir_ziel2";

    fs::remove_all(source_root);
    fs::remove_all(target1_root);
    fs::remove_all(target2_root);
    fs::create_directories(source_root);
    // SyncDir legt nur die letzte Ebene des Ziels an (create_target_dir_if_not_exist
    // ruft fs::create_directory(), nicht create_directories()); die Eltern
    // muessen also schon existieren.
    fs::create_directories(target1_root);
    fs::create_directories(target2_root);

    Synchronize::start();
    for (const auto& name : DIFFICULT_DIR_NAMES) {
        const fs::path source_dir = source_root / name;
        fs::create_directories(source_dir);
        std::ofstream(source_dir / "inhalt.txt") << "inhalt";

        for (const fs::path& target_root : { target1_root, target2_root }) {
            const fs::path target_dir = target_root / name;
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

// SyncTree rekursiert selbst durch die Verzeichnisstruktur - die schwierigen
// Namen stecken hier als Unterverzeichnisse in EINEM Baum, den SyncTree
// selbst per fs::directory_iterator entdecken und im Ziel nachbilden muss.
TEST(DifficultDirnameTests, TreeWithDifficultSubdirNamesSyncsToTwoTargets)
{
    const fs::path source_root = fs::path(util::wstring_to_string(BASE_DIR)) / "sync_tree_quelle";
    const fs::path target1_root = fs::path(util::wstring_to_string(BASE_DIR)) / "sync_tree_ziel1";
    const fs::path target2_root = fs::path(util::wstring_to_string(BASE_DIR)) / "sync_tree_ziel2";

    fs::remove_all(source_root);
    fs::remove_all(target1_root);
    fs::remove_all(target2_root);
    fs::create_directories(source_root);
    for (const auto& name : DIFFICULT_DIR_NAMES) {
        fs::create_directories(source_root / name);
        std::ofstream(source_root / name / "datei.txt") << "inhalt";
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
            EXPECT_TRUE(fs::exists(target_root / name / "datei.txt")) << "Verzeichnisname: " << name;
    }

    fs::remove_all(source_root);
    fs::remove_all(target1_root);
    fs::remove_all(target2_root);
}
