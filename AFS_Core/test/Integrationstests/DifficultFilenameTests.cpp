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
    const wstring BASE_DIR = TestPaths::scratch_dir() + L"/difficult_filenames";  // [DifficultFilenameTests.md#BASE_DIR]
    const wstring SOURCE_DIR = BASE_DIR + L"/quelle";
    const wstring TARGET1_DIR = BASE_DIR + L"/ziel1";
    const wstring TARGET2_DIR = BASE_DIR + L"/ziel2";

    const std::vector<string> DIFFICULT_NAMES = {  // [DifficultFilenameTests.md#DIFFICULT_NAMES]
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

    void create_empty_files(const fs::path& dir)
    {
        fs::create_directories(dir);
        for (const auto& name : DIFFICULT_NAMES)
            std::ofstream(dir / name).close();
    }

    void expect_all_present_and_empty(const fs::path& dir)  // [DifficultFilenameTests.md#expect_all_present_and_empty]
    {
        for (const auto& name : DIFFICULT_NAMES) {
            const fs::path file = dir / name;
            EXPECT_TRUE(fs::exists(file)) << "fehlt: " << name;
            if (fs::exists(file)) {
                EXPECT_EQ(fs::file_size(file), 0u) << "nicht leer: " << name;
            }
        }
    }
}

TEST(DifficultFilenameTests, EmptyFilesWithDifficultNamesSyncToTwoTargets)  // [DifficultFilenameTests.md#EmptyFilesWithDifficultNamesSyncToTwoTargets]
{
    fs::remove_all(BASE_DIR);
    create_empty_files(SOURCE_DIR);

    Synchronize::start();
    for (const wstring& target : { TARGET1_DIR, TARGET2_DIR }) {
        SyncItem si;
        si.sync_cycle(0);
        si.sync_item_type(SyncItemType::Dir);
        si.source(SOURCE_DIR);
        si.target(target);
        si.sync_empty_files(true);  // [DifficultFilenameTests.md#sync-empty-files-needed]
        std::size_t copy_count = 0;
        SyncDir sync(si, copy_count, false);
        sync();
        EXPECT_EQ(copy_count, DIFFICULT_NAMES.size());
    }

    expect_all_present_and_empty(TARGET1_DIR);
    expect_all_present_and_empty(TARGET2_DIR);

    fs::remove_all(BASE_DIR);
}
