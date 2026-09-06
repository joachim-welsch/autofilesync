#include "pch.h"
#include "SyncDir.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace AFS_Core;

namespace fs = std::filesystem;
using std::wstring;
using std::string;

namespace {
    // Alle drei Verzeichnisse liegen unter dem Scratch-Verzeichnis (ein
    // Unterverzeichnis von /tmp) und sind selbst ASCII-only - die schwierigen
    // Zeichen stecken ausschliesslich in den Dateinamen darunter
    // (difficultNames), nie im Verzeichnispfad. So bleibt jede
    // wstring-Konvertierung des Verzeichnispfads unkritisch; nur der native
    // (UTF-8) Dateiname selbst muss die Sonderfaelle ueberstehen.
    const wstring baseDir = TestPaths::scratchDir() + L"/difficult_filenames";
    const wstring sourceDir = baseDir + L"/quelle";
    const wstring target1Dir = baseDir + L"/ziel1";
    const wstring target2Dir = baseDir + L"/ziel2";

    // Absichtlich moeglichst schwierige, aber unter Linux gueltige Dateinamen
    // (nur '/' und NUL sind dort in einem Dateinamen wirklich verboten): sehr
    // lang, mit Leerzeichen (fuehrend/mehrfach/abschliessend), Shell-
    // Sonderzeichen samt eingebettetem Backslash, ein verstecktes Dotfile,
    // ein Name nur aus Punkten, Unicode/Emoji, ein eingebetteter
    // Zeilenumbruch, ein abschliessender Punkt und ein fuehrender
    // Bindestrich (koennte in einer Shell faelschlich als Option gelesen
    // werden).
    const std::vector<string> difficultNames = {
        string(200, 'a') + ".txt",
        "Datei mit Leerzeichen.txt",
        "   mehrfache   Leerzeichen   .txt",
        "datei_mit_leerzeichen_am_ende.txt ",
        "special!@#$%^&*()_+-=[]{};'\\,~`<>?.txt",
        ".versteckte_datei.txt",
        "...nur_punkte....txt",
        "Ünïcödé_日本語_😀_Prüfung.txt",
        "zeilen\numbruch.txt",
        "abschliessender_punkt.txt.",
        "-rf.txt",
    };

    void createEmptyFiles(const fs::path& dir)
    {
        fs::create_directories(dir);
        for (const auto& name : difficultNames)
            std::ofstream(dir / name).close();
    }

    // Prueft, dass jeder schwierige Name im Zielverzeichnis als leere Datei
    // angekommen ist.
    void expectAllPresentAndEmpty(const fs::path& dir)
    {
        for (const auto& name : difficultNames) {
            const fs::path file = dir / name;
            EXPECT_TRUE(fs::exists(file)) << "fehlt: " << name;
            if (fs::exists(file)) {
                EXPECT_EQ(fs::file_size(file), 0u) << "nicht leer: " << name;
            }
        }
    }
}

// Kopiert eine leere Datei unter moeglichst vielen schwierigen Dateinamen
// (lang, Leerzeichen, Sonderzeichen, Unicode, ...) von einem Quell- in zwei
// verschiedene Zielverzeichnisse unter /tmp und prueft, dass alle Namen
// unversehrt und leer ankommen.
TEST(DifficultFilenameTests, EmptyFilesWithDifficultNamesSyncToTwoTargets)
{
    fs::remove_all(baseDir);
    createEmptyFiles(sourceDir);

    Synchronize::start();
    for (const wstring& target : { target1Dir, target2Dir }) {
        SyncItem si;
        si.syncCycle(0);
        si.syncItemType(SyncItemType::Dir);
        si.source(sourceDir);
        si.target(target);
        // Ohne dieses Flag werden 0-Byte-Quelldateien absichtlich
        // uebersprungen (siehe SyncFileTests: leere Dateien gelten sonst als
        // abgebrochener Schreibvorgang) - hier sind sie aber gewollt leer.
        si.syncEmptyFiles(true);
        std::size_t copyCount = 0;
        SyncDir sync(si, copyCount, false);
        sync();
        EXPECT_EQ(copyCount, difficultNames.size());
    }

    expectAllPresentAndEmpty(target1Dir);
    expectAllPresentAndEmpty(target2Dir);

    fs::remove_all(baseDir);
}
