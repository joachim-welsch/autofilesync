/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
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
        "Datei mit Leerzeichen.txt",
        "   mehrfache   Leerzeichen   .txt",
        ".versteckte_datei.txt",
        "...nur_punkte....txt",
        "Ünïcödé_日本語_😀_Prüfung.txt",
        "-rf.txt",
#ifdef _UNIX
        string(200, 'a') + ".txt",
        "datei_mit_leerzeichen_am_ende.txt ",
        "special!@#$%^&*()_+-=[]{};'\\,~`<>?.txt",
        "zeilen\numbruch.txt",
        "abschliessender_punkt.txt.",
#else
        string(150, 'a') + ".txt",  // [DifficultFilenameTests.md#windows-names]
        "special!@#$%^&()_+-=[]{};',~`.txt",
#endif
    };

    fs::path from_utf8(const string& name)  // [DifficultFilenameTests.md#utf8-paths]
    {
        return fs::path(std::u8string(name.begin(), name.end()));
    }

    void create_empty_files(const fs::path& dir)
    {
        fs::create_directories(dir);
        for (const auto& name : DIFFICULT_NAMES)
            std::ofstream(dir / from_utf8(name)).close();
    }

    void expect_all_present_and_empty(const fs::path& dir)  // [DifficultFilenameTests.md#expect_all_present_and_empty]
    {
        for (const auto& name : DIFFICULT_NAMES) {
            const fs::path file = dir / from_utf8(name);
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
