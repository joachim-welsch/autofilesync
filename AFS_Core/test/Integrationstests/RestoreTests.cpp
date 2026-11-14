/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include "Restore.h"
#include "FileDelete.h"
#include "RecycleBin.h"
#include "ScopedDeleteMode.h"
#include "ScopedRecycleBin.h"
#include "TestPaths.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

using namespace AFS_Core;
namespace fs = std::filesystem;
using std::chrono::system_clock;

namespace {

class RestoreTest : public ::testing::Test {  // [RestoreTests.md#RestoreTest]
protected:
    tu::ScopedRecycleBin bin_;
    tu::ScopedDeleteMode delete_mode_{DeleteMode::RecycleBin};
    fs::path root_;
    fs::path source_;
    fs::path target_;
    std::atomic<bool> stop_{false};

    void SetUp() override
    {
        const std::string test_name = testing::UnitTest::GetInstance()->current_test_info()->name();
        root_ = fs::temp_directory_path() / ("autofilesync_restoretest_" + test_name + "_" + std::to_string(TestPaths::process_id()));
        fs::remove_all(root_);
        fs::create_directories(root_);
        root_ = fs::canonical(root_);
        source_ = root_ / "quelle";
        target_ = root_ / "ziel";
        fs::create_directories(source_);
        fs::create_directories(target_);
    }

    void TearDown() override
    {
        fs::remove_all(root_);
    }

    static void write(const fs::path& file, const std::string& content)
    {
        fs::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary) << content;
    }

    static std::string content_of(const fs::path& file)
    {
        std::ifstream in(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), {});
    }

    SyncItem item(SyncItemType type, bool keeps_versions = false) const
    {
        SyncItem made(L"Test", type, source_.wstring(), target_.wstring(), L"*");
        made.father_son_principle(keeps_versions);
        return made;
    }

    SyncItem file_item(const fs::path& source_file) const
    {
        SyncItem made(L"Test", SyncItemType::File, source_file.wstring(), target_.wstring(), L"");
        made.father_son_principle(true);
        return made;
    }

    Restorables find(const SyncItems& items = {}) const
    {
        return restore::find(items, stop_);
    }

    void delete_through_bin(const fs::path& file, const std::string& content = "inhalt") const
    {
        write(file, content);
        ASSERT_TRUE(delete_file(file.wstring()));
        ASSERT_FALSE(fs::exists(file));
    }
};

TEST_F(RestoreTest, NothingDeletedAndNoVersionsFindsNothing)
{
    write(target_ / "bericht.txt", "aktuell");
    EXPECT_TRUE(find({item(SyncItemType::Tree, true)}).empty());
}

TEST_F(RestoreTest, DeletedFileOfATargetBelongsIntoTheSource)  // [RestoreTests.md#DeletedFileOfATargetBelongsIntoTheSource]
{
    const auto before = system_clock::now() - std::chrono::seconds(1);
    delete_through_bin(target_ / "briefe" / "anna.txt", "liebe anna");
    const Restorables found = find({item(SyncItemType::Tree)});
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].kind, RestorableKind::Deleted);
    EXPECT_EQ(found[0].home, source_ / "briefe" / "anna.txt");
    EXPECT_EQ(found[0].size, 10u);
    EXPECT_GE(found[0].time, before);
    EXPECT_LE(found[0].time, system_clock::now());
    EXPECT_TRUE(restore::place_known(found[0]));

    const auto restored = restore::put_back(found[0], found[0].home.parent_path());
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(*restored, source_ / "briefe" / "anna.txt");
    EXPECT_EQ(content_of(*restored), "liebe anna");
    EXPECT_TRUE(find({item(SyncItemType::Tree)}).empty()) << "The restored file is still offered.";
}

#ifndef _WIN32
TEST_F(RestoreTest, DeletedFileOutsideEveryItemBelongsWhereItWas)
{
    delete_through_bin(root_ / "anderswo" / "notiz.txt");
    const Restorables found = find({item(SyncItemType::Tree)});
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].home, root_ / "anderswo" / "notiz.txt");
}
#endif

TEST_F(RestoreTest, DirItemTakesOnlyItsOwnFilesIntoTheSource)
{
    delete_through_bin(target_ / "oben.txt");
    delete_through_bin(target_ / "tiefer" / "unten.txt");
    const Restorables found = find({item(SyncItemType::Dir)});
    ASSERT_EQ(found.size(), 2u);
    const auto home_of = [&](const std::string& name) {
        return std::ranges::find_if(found, [&](const Restorable& file) { return file.home.filename() == name; })->home;
    };
    EXPECT_EQ(home_of("oben.txt"), source_ / "oben.txt");
    EXPECT_NE(home_of("unten.txt"), source_ / "tiefer" / "unten.txt");
}

TEST_F(RestoreTest, FileItemTakesItsFileBackToTheSourceFile)
{
    const fs::path source_file = root_ / "arbeit" / "plan.txt";
    delete_through_bin(target_ / "plan.txt");
    delete_through_bin(target_ / "fremd.txt");
    const Restorables found = find({file_item(source_file)});
    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(std::ranges::count_if(found, [&](const Restorable& file) { return file.home == source_file; }), 1);
}

TEST_F(RestoreTest, DeepestTargetDecidesBetweenNestedItems)
{
    SyncItem inner(L"Innen", SyncItemType::Tree, (root_ / "innenquelle").wstring(), (target_ / "innen").wstring(), L"*");
    delete_through_bin(target_ / "innen" / "datei.txt");
    const Restorables found = find({item(SyncItemType::Tree), inner});
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].home, root_ / "innenquelle" / "datei.txt");
}

TEST_F(RestoreTest, ExistingFileIsNeverOverwritten)  // [RestoreTests.md#ExistingFileIsNeverOverwritten]
{
    delete_through_bin(target_ / "bericht.txt", "erste");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));  // [RestoreTests.md#ExistingFileIsNeverOverwritten]
    delete_through_bin(target_ / "bericht.txt", "zweite");
    write(source_ / "bericht.txt", "neu");
    Restorables found = find({item(SyncItemType::Tree)});
    ASSERT_EQ(found.size(), 2u);
    found[1].time = found[0].time;

    const auto first = restore::put_back(found[0], source_);
    const auto second = restore::put_back(found[1], source_);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(content_of(source_ / "bericht.txt"), "neu");
    EXPECT_NE(first->filename(), "bericht.txt");
    EXPECT_NE(*first, *second);
    EXPECT_EQ(first->extension(), ".txt");
    EXPECT_TRUE(first->filename().wstring().starts_with(L"bericht (")) << first->filename();
    EXPECT_EQ(content_of(*first) + content_of(*second), "zweiteerste");
}

TEST_F(RestoreTest, PutBackIntoAnotherDirectoryCreatesIt)
{
    delete_through_bin(target_ / "bild.jpg", "jpeg");
    const Restorables found = find();
    ASSERT_EQ(found.size(), 1u);
    const auto restored = restore::put_back(found[0], root_ / "gerettet" / "heute");
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(*restored, root_ / "gerettet" / "heute" / "bild.jpg");
    EXPECT_EQ(content_of(*restored), "jpeg");
}

TEST_F(RestoreTest, FilesOfADeletedDirectoryKeepTheirPlace)  // [RestoreTests.md#FilesOfADeletedDirectoryKeepTheirPlace]
{
    write(target_ / "alben" / "urlaub" / "strand.jpg", "sand");
    ASSERT_TRUE(RecycleBin::instance().add_item(target_ / "alben", (target_.relative_path() / "2026-03-04_05-06-07-089_alben")));
    const Restorables found = find({item(SyncItemType::Tree)});
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].home, source_ / "alben" / "urlaub" / "strand.jpg");
}

TEST_F(RestoreTest, FilesWithoutTimestampInTheBinAreNotOffered)
{
    write(bin_.path() / "fremd" / "irgendwas.txt", "nicht von uns");
    EXPECT_TRUE(find().empty());
}

TEST_F(RestoreTest, EarlierVersionsNeedTheFatherSonPrinciple)
{
    write(target_ / "bericht.txt.bak.1", "alt");
    EXPECT_TRUE(find({item(SyncItemType::Tree, false)}).empty());
    EXPECT_EQ(find({item(SyncItemType::Tree, true)}).size(), 1u);
}

TEST_F(RestoreTest, EarlierVersionIsCopiedBesideTheCurrentFile)  // [RestoreTests.md#EarlierVersionIsCopiedBesideTheCurrentFile]
{
    write(source_ / "texte" / "bericht.txt", "aktuell");
    write(target_ / "texte" / "bericht.txt", "aktuell");
    write(target_ / "texte" / "bericht.txt.bak.1", "alt");
    const auto long_ago = fs::file_time_type::clock::now() - std::chrono::hours(48);
    fs::last_write_time(target_ / "texte" / "bericht.txt.bak.1", long_ago);
    const Restorables found = find({item(SyncItemType::Tree, true)});
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].kind, RestorableKind::EarlierVersion);
    EXPECT_EQ(found[0].home, source_ / "texte" / "bericht.txt");
    EXPECT_LT(found[0].time, system_clock::now() - std::chrono::hours(47));

    const auto restored = restore::put_back(found[0], found[0].home.parent_path());
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->parent_path(), source_ / "texte");
    EXPECT_NE(restored->filename(), "bericht.txt");
    EXPECT_EQ(content_of(*restored), "alt");
    EXPECT_EQ(content_of(source_ / "texte" / "bericht.txt"), "aktuell");
    EXPECT_EQ(content_of(target_ / "texte" / "bericht.txt.bak.1"), "alt") << "The version must stay in the target.";
    EXPECT_GT(fs::last_write_time(*restored), long_ago + std::chrono::hours(47)) << "The copy must count as the newest file.";
    EXPECT_EQ(find({item(SyncItemType::Tree, true)}).size(), 1u);
}

TEST_F(RestoreTest, EarlierVersionOfAMissingFileGetsItsName)
{
    write(target_ / "bericht.txt.bak.2", "alt");
    const Restorables found = find({item(SyncItemType::Tree, true)});
    ASSERT_EQ(found.size(), 1u);
    const auto restored = restore::put_back(found[0], source_);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(*restored, source_ / "bericht.txt");
}

TEST_F(RestoreTest, DirItemLooksForVersionsOnlyInItsDirectory)
{
    write(target_ / "oben.txt.bak.1", "alt");
    write(target_ / "tiefer" / "unten.txt.bak.1", "alt");
    EXPECT_EQ(find({item(SyncItemType::Dir, true)}).size(), 1u);
    EXPECT_EQ(find({item(SyncItemType::Tree, true)}).size(), 2u);
}

TEST_F(RestoreTest, NamesThatOnlyResembleVersionsAreNotOffered)
{
    write(target_ / "sicherung.bak", "x");
    write(target_ / "sicherung.bak.neu", "x");
    write(target_ / ".bak.1", "x");
    EXPECT_TRUE(find({item(SyncItemType::Tree, true)}).empty());
}

TEST_F(RestoreTest, NewestComesFirst)
{
    write(target_ / "a.txt.bak.1", "alt");
    fs::last_write_time(target_ / "a.txt.bak.1", fs::file_time_type::clock::now() - std::chrono::hours(5));
    delete_through_bin(target_ / "b.txt");
    const Restorables found = find({item(SyncItemType::Tree, true)});
    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found[0].kind, RestorableKind::Deleted);
    EXPECT_EQ(found[1].kind, RestorableKind::EarlierVersion);
}

TEST_F(RestoreTest, StopEndsTheSearchForVersions)
{
    write(target_ / "a.txt.bak.1", "alt");
    stop_ = true;
    EXPECT_TRUE(find({item(SyncItemType::Tree, true)}).empty());
}

TEST_F(RestoreTest, MissingTargetDirectoryFindsNothing)
{
    fs::remove_all(target_);
    EXPECT_TRUE(find({item(SyncItemType::Tree, true)}).empty());
}

TEST_F(RestoreTest, FileGoneFromTheBinFailsWithoutCreatingAnything)
{
    delete_through_bin(target_ / "weg.txt");
    const Restorables found = find();
    ASSERT_EQ(found.size(), 1u);
    fs::remove(found[0].stored);
    EXPECT_FALSE(restore::put_back(found[0], source_).has_value());
    EXPECT_TRUE(fs::is_empty(source_));
}

TEST_F(RestoreTest, BinRefusesToTakeAnItemOntoAnExistingFile)
{
    delete_through_bin(target_ / "da.txt", "aus dem papierkorb");
    write(source_ / "da.txt", "vorhanden");
    const fs::path item = RecycleBin::instance().list_items().front();
    EXPECT_FALSE(RecycleBin::instance().take_item(item, source_ / "da.txt"));
    EXPECT_EQ(content_of(source_ / "da.txt"), "vorhanden");
    EXPECT_EQ(RecycleBin::instance().list_items().size(), 1u);
}

}
