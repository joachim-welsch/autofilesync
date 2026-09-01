#include "pch.h"
#include "SyncDir.h"
#include "SyncTree.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "util.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using std::wstring;
using std::string;

namespace {
    const wstring baseDir = TestPaths::scratchDir() + L"/difficult_dirnames";

    // Gleiche Kategorien wie DifficultFilenameTests, aber ohne Dateiendung und
    // ohne eingebetteten Backslash: der wird von dieser App an mehreren
    // Stellen als Pfadtrenner interpretiert (DIR_SEPARATOR, win_to_sftp_path,
    // ...), ein Backslash *im Verzeichnisnamen* wuerde also Trennzeichen- statt
    // Namensverarbeitung testen - ein anderes Thema als hier.
    const std::vector<string> difficultDirNames = {
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
    wstring toSyncPath(const fs::path& p)
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
    const fs::path sourceRoot = fs::path(util::wstring_to_string(baseDir)) / "sync_dir_quelle";
    const fs::path target1Root = fs::path(util::wstring_to_string(baseDir)) / "sync_dir_ziel1";
    const fs::path target2Root = fs::path(util::wstring_to_string(baseDir)) / "sync_dir_ziel2";

    fs::remove_all(sourceRoot);
    fs::remove_all(target1Root);
    fs::remove_all(target2Root);
    fs::create_directories(sourceRoot);
    // SyncDir legt nur die letzte Ebene des Ziels an (createTargetDirIfNotExist
    // ruft fs::create_directory(), nicht create_directories()); die Eltern
    // muessen also schon existieren.
    fs::create_directories(target1Root);
    fs::create_directories(target2Root);

    Synchronize::start();
    for (const auto& name : difficultDirNames) {
        const fs::path sourceDir = sourceRoot / name;
        fs::create_directories(sourceDir);
        std::ofstream(sourceDir / "inhalt.txt") << "inhalt";

        for (const fs::path& targetRoot : { target1Root, target2Root }) {
            const fs::path targetDir = targetRoot / name;
            SyncItem si;
            si.syncCycle(0);
            si.syncItemType(SyncItemType::Dir);
            si.source(toSyncPath(sourceDir));
            si.target(toSyncPath(targetDir));
            std::size_t copyCount = 0;
            SyncDir sync(si, copyCount, false);
            sync();
            EXPECT_EQ(copyCount, 1u) << "Verzeichnisname: " << name;
            EXPECT_TRUE(fs::exists(targetDir / "inhalt.txt")) << "Verzeichnisname: " << name;
        }
    }

    fs::remove_all(sourceRoot);
    fs::remove_all(target1Root);
    fs::remove_all(target2Root);
}

// SyncTree rekursiert selbst durch die Verzeichnisstruktur - die schwierigen
// Namen stecken hier als Unterverzeichnisse in EINEM Baum, den SyncTree
// selbst per fs::directory_iterator entdecken und im Ziel nachbilden muss.
TEST(DifficultDirnameTests, TreeWithDifficultSubdirNamesSyncsToTwoTargets)
{
    const fs::path sourceRoot = fs::path(util::wstring_to_string(baseDir)) / "sync_tree_quelle";
    const fs::path target1Root = fs::path(util::wstring_to_string(baseDir)) / "sync_tree_ziel1";
    const fs::path target2Root = fs::path(util::wstring_to_string(baseDir)) / "sync_tree_ziel2";

    fs::remove_all(sourceRoot);
    fs::remove_all(target1Root);
    fs::remove_all(target2Root);
    fs::create_directories(sourceRoot);
    for (const auto& name : difficultDirNames) {
        fs::create_directories(sourceRoot / name);
        std::ofstream(sourceRoot / name / "datei.txt") << "inhalt";
    }

    Synchronize::start();
    for (const fs::path& targetRoot : { target1Root, target2Root }) {
        SyncItem si;
        si.syncCycle(0);
        si.syncItemType(SyncItemType::Tree);
        si.source(toSyncPath(sourceRoot));
        si.target(toSyncPath(targetRoot));
        std::size_t copyCount = 0;
        SyncTree sync(si, copyCount, false);
        sync();
        EXPECT_EQ(copyCount, difficultDirNames.size());

        for (const auto& name : difficultDirNames)
            EXPECT_TRUE(fs::exists(targetRoot / name / "datei.txt")) << "Verzeichnisname: " << name;
    }

    fs::remove_all(sourceRoot);
    fs::remove_all(target1Root);
    fs::remove_all(target2Root);
}
