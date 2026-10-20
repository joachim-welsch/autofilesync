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
#include "SyncState.h"
#include "Config.h"
#include "Util.h"
#include "ScopedEnv.h"
#include "ScopedLoggingSetup.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

using namespace AFS_Core;

namespace fs = std::filesystem;
using std::wstring;

namespace {
    std::string file_bytes(const fs::path& file)
    {
        std::ifstream in(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), {});
    }

    FileState state_at(uint64_t size, int64_t source_ns, int64_t target_ns)
    {
        return {size, util::from_unix_nanoseconds(source_ns), util::from_unix_nanoseconds(target_ns)};
    }

    class SyncStateTest: public ::testing::Test {
    protected:
        const wstring source_ = L"/home/x/Dokumente";
        const wstring target_ = L"/media/backup/Dokumente";
        const fs::path test_dir_ = fs::temp_directory_path() / "autofilesync_syncstate_test";
        tu::ScopedLoggingSetup logging_;
        std::unique_ptr<ScopedEnv> config_home_;  // [SyncStateTests.md#config-dir-in-temp]

        void SetUp() override
        {
            fs::remove_all(test_dir_);
            fs::create_directories(test_dir_);
            config_home_ = std::make_unique<ScopedEnv>("XDG_CONFIG_HOME", test_dir_.string().c_str());
            Logging::instance().set_log_file_path_name((test_dir_ / "test.log").wstring());
        }
        void TearDown() override
        {
            config_home_.reset();
            fs::remove_all(test_dir_);
        }
        fs::path state_file() const { return test_dir_ / "state.tsv"; }
        SyncState filled_state() const
        {
            SyncState state(source_, target_);
            state.record(L"Grüße/Äpfel.txt", state_at(1234, 1'700'000'000'123'456'789, 1'700'000'000'000'000'000));
            state.record(L"Фото/Київ.jpg", state_at(0, 0, -1'000'000'000));
            state.record(L"emoji 🙂/Datei.bin", state_at(UINT64_C(5'000'000'000), 1'800'000'000'999'999'999, 1'800'000'001'000'000'000));
            return state;
        }
        void write_text(const wstring& text) const { util::write_utf8_file(state_file(), text); }
        wstring header() const { return L"autofilesync-sync-state 1\nsource\t" + source_ + L"\ntarget\t" + target_ + L"\n"; }
        std::optional<SyncState> read() const { return read_sync_state(state_file(), source_, target_); }
    };
}

TEST_F(SyncStateTest, NoFileMeansNoState)
{
    EXPECT_FALSE(read().has_value());
    EXPECT_FALSE(fs::exists(state_file()));
}

TEST_F(SyncStateTest, RoundTripKeepsEveryEntry)  // [SyncStateTests.md#RoundTripKeepsEveryEntry]
{
    const SyncState written = filled_state();
    ASSERT_TRUE(write_sync_state(state_file(), written));
    const std::optional<SyncState> read = this->read();
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read->source(), source_);
    EXPECT_EQ(read->target(), target_);
    EXPECT_EQ(read->files(), written.files());
}

TEST_F(SyncStateTest, TabsBackslashesAndLineBreaksInPathsSurvive)  // [SyncStateTests.md#TabsBackslashesAndLineBreaksInPathsSurvive]
{
    SyncState written(L"C:\\Users\\x\tDokumente", L"\\\\nas\\backup\n");
    written.record(L"Ordner\\t/Datei\tmit\nUmbruch\r\\n.txt", state_at(1, 2, 3));
    written.record(L"\\", state_at(4, 5, 6));
    ASSERT_TRUE(write_sync_state(state_file(), written));
    const std::optional<SyncState> read = read_sync_state(state_file(), written.source(), written.target());
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read->files(), written.files());
}

TEST_F(SyncStateTest, FileIsUtf8WithCharactersAsThemselves)
{
    ASSERT_TRUE(write_sync_state(state_file(), filled_state()));
    const std::string bytes = file_bytes(state_file());
    EXPECT_NE(bytes.find("\tGrüße/Äpfel.txt\n"), std::string::npos);
    EXPECT_NE(bytes.find("\tФото/Київ.jpg\n"), std::string::npos);
    EXPECT_NE(bytes.find("\temoji 🙂/Datei.bin\n"), std::string::npos);
    EXPECT_EQ(bytes.rfind("autofilesync-sync-state 1\n", 0), 0u);
}

TEST_F(SyncStateTest, StateOfOtherPathsIsIgnored)  // [SyncStateTests.md#StateOfOtherPathsIsIgnored]
{
    ASSERT_TRUE(write_sync_state(state_file(), filled_state()));
    EXPECT_FALSE(read_sync_state(state_file(), source_, L"/media/other/Dokumente").has_value());
    EXPECT_FALSE(read_sync_state(state_file(), L"/home/y/Dokumente", target_).has_value());
    EXPECT_TRUE(read().has_value());
}

TEST_F(SyncStateTest, UnreadableFileMeansNoState)  // [SyncStateTests.md#UnreadableFileMeansNoState]
{
    write_text(L"{ \"version\": \"1\" }");
    EXPECT_FALSE(read().has_value());
    write_text(L"autofilesync-sync-state 1\nsource\t" + source_ + L"\n");
    EXPECT_FALSE(read().has_value());
    write_text(header() + L"12\t34\tDatei.txt\n");
    EXPECT_FALSE(read().has_value());
    write_text(header() + L"12\t34\t5٦\tDatei.txt\n");
    EXPECT_FALSE(read().has_value());
    write_text(header() + L"12\t34\t56\tDatei.txt\n");
    EXPECT_TRUE(read().has_value());
    EXPECT_TRUE(fs::exists(state_file()));
}

TEST_F(SyncStateTest, FileOfANewerFormatIsNotTrusted)
{
    write_text(L"autofilesync-sync-state 2\nsource\t" + source_ + L"\ntarget\t" + target_ + L"\n");
    EXPECT_FALSE(read().has_value());
}

TEST_F(SyncStateTest, EmptyStateRoundTrips)
{
    ASSERT_TRUE(write_sync_state(state_file(), SyncState(source_, target_)));
    const std::optional<SyncState> read = this->read();
    ASSERT_TRUE(read.has_value());
    EXPECT_TRUE(read->empty());
}

TEST_F(SyncStateTest, WriteCreatesTheDirectoryAndLeavesNoTempFile)
{
    const fs::path file = test_dir_ / "a" / "b" / "state.tsv";
    ASSERT_TRUE(write_sync_state(file, filled_state()));
    EXPECT_TRUE(fs::exists(file));
    EXPECT_FALSE(fs::exists(file.wstring() + L".tmp"));
}

TEST_F(SyncStateTest, FailedWriteLeavesNoTempFileAndNoHalfFile)  // [SyncStateTests.md#FailedWriteLeavesNoTempFileAndNoHalfFile]
{
    const fs::path blocking_file = test_dir_ / "file";
    util::write_utf8_file(blocking_file, L"not a directory");
    const fs::path file = blocking_file / "state.tsv";
    EXPECT_FALSE(write_sync_state(file, filled_state()));
    EXPECT_FALSE(fs::exists(file.wstring() + L".tmp"));
    EXPECT_TRUE(fs::is_regular_file(blocking_file));
}

TEST_F(SyncStateTest, OverwriteReplacesThePreviousStateAsAWhole)
{
    ASSERT_TRUE(write_sync_state(state_file(), filled_state()));
    SyncState smaller(source_, target_);
    smaller.record(L"only.txt", state_at(1, 2, 3));
    ASSERT_TRUE(write_sync_state(state_file(), smaller));
    const std::optional<SyncState> read = this->read();
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read->files(), smaller.files());
}

TEST_F(SyncStateTest, FindReturnsTheEntryOrNothing)
{
    const SyncState state = filled_state();
    ASSERT_NE(state.find(L"Grüße/Äpfel.txt"), nullptr);
    EXPECT_EQ(state.find(L"Grüße/Äpfel.txt")->size, 1234u);
    EXPECT_EQ(state.find(L"Grüße/Äpfel.TXT"), nullptr);
    EXPECT_EQ(state.find(L"fehlt.txt"), nullptr);
}

TEST_F(SyncStateTest, ChangedOnlyWhenSomethingDiffers)  // [SyncStateTests.md#ChangedOnlyWhenSomethingDiffers]
{
    ASSERT_TRUE(write_sync_state(state_file(), filled_state()));
    std::optional<SyncState> state = read();
    ASSERT_TRUE(state.has_value());
    EXPECT_FALSE(state->changed());
    state->seen(L"Grüße/Äpfel.txt");
    state->record(L"Grüße/Äpfel.txt", *state->find(L"Grüße/Äpfel.txt"));
    state->forget(L"fehlt.txt");
    EXPECT_FALSE(state->changed());
    state->record(L"Grüße/Äpfel.txt", state_at(1235, 1, 2));
    EXPECT_TRUE(state->changed());
    SyncState fresh(source_, target_);
    EXPECT_FALSE(fresh.changed());
    fresh.forget_unseen();
    EXPECT_FALSE(fresh.changed());
    fresh.record(L"neu.txt", state_at(1, 2, 3));
    EXPECT_TRUE(fresh.changed());
}

TEST_F(SyncStateTest, ForgetUnseenDropsOnlyTheUntouched)  // [SyncStateTests.md#ForgetUnseenDropsOnlyTheUntouched]
{
    ASSERT_TRUE(write_sync_state(state_file(), filled_state()));
    std::optional<SyncState> state = read();
    ASSERT_TRUE(state.has_value());
    state->seen(L"Grüße/Äpfel.txt");
    state->record(L"neu.txt", state_at(7, 8, 9));
    state->forget_unseen();
    EXPECT_EQ(state->files().size(), 2u);
    EXPECT_NE(state->find(L"Grüße/Äpfel.txt"), nullptr);
    EXPECT_NE(state->find(L"neu.txt"), nullptr);
    EXPECT_TRUE(state->changed());
    state->forget(L"neu.txt");
    EXPECT_EQ(state->find(L"neu.txt"), nullptr);
    state->forget_unseen();
    EXPECT_TRUE(state->empty());
}

TEST_F(SyncStateTest, StateFileIsNamedAfterTheSanitisedItemName)
{
    EXPECT_EQ(sync_state_file_path(test_dir_, L"Fotos: Urlaub/2026 ").filename(), fs::path(L"Fotos_ Urlaub_2026.tsv"));
    EXPECT_EQ(sync_state_file_path(test_dir_, L"Grüße").filename(), fs::path(L"Grüße.tsv"));
    EXPECT_EQ(sync_state_file_path(test_dir_, L"...").filename(), fs::path(L"state.tsv"));
}

TEST_F(SyncStateTest, StateDirIsNextToTheConfiguration)
{
    EXPECT_EQ(sync_state_dir(), fs::path(Config::get_root_pathname()).parent_path() / L"state");
    EXPECT_EQ(sync_state_dir().filename(), fs::path(L"state"));
}

TEST_F(SyncStateTest, RemoveDeletesTheFile)
{
    ASSERT_TRUE(write_sync_state(state_file(), filled_state()));
    EXPECT_TRUE(remove_sync_state(state_file()));
    EXPECT_FALSE(fs::exists(state_file()));
    EXPECT_TRUE(remove_sync_state(state_file()));
}

TEST_F(SyncStateTest, ALargeTreeLoadsInReasonableTime)  // [SyncStateTests.md#ALargeTreeLoadsInReasonableTime]
{
    using namespace std::chrono;
    constexpr std::size_t FILES = 100'000;
    SyncState state(source_, target_);
    for (std::size_t i = 0; i < FILES; ++i)
        state.record(L"Ordner " + std::to_wstring(i % 100) + L"/Datei " + std::to_wstring(i) + L".txt",
            state_at(i, 1'700'000'000'000'000'000 + static_cast<int64_t>(i), 1'700'000'000'000'000'000 + static_cast<int64_t>(i)));
    const auto start = steady_clock::now();
    ASSERT_TRUE(write_sync_state(state_file(), state));
    const auto written = steady_clock::now();
    const std::optional<SyncState> read = this->read();
    const auto done = steady_clock::now();
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read->files(), state.files());
    std::cout << "write " << duration_cast<milliseconds>(written - start).count() << " ms, read "
        << duration_cast<milliseconds>(done - written).count() << " ms, "
        << fs::file_size(state_file()) / 1024 << " KiB\n";
    EXPECT_LT(duration_cast<seconds>(done - start).count(), 5);
}

TEST_F(SyncStateTest, KnowsBelowSeesOnlyFilesUnderTheDirectory)  // [SyncStateTests.md#KnowsBelowSeesOnlyFilesUnderTheDirectory]
{
    SyncState state = filled_state();
    state.record(L"a/b/c.txt", state_at(1, 2, 3));
    EXPECT_TRUE(state.knows_below(L"Grüße"));
    EXPECT_TRUE(state.knows_below(L"a"));
    EXPECT_TRUE(state.knows_below(L"a/b"));
    EXPECT_FALSE(state.knows_below(L"Grü"));
    EXPECT_FALSE(state.knows_below(L"a/bc"));
    EXPECT_FALSE(state.knows_below(L"a/b/c.txt"));
    EXPECT_FALSE(state.knows_below(L"fehlt"));
    state.forget(L"a/b/c.txt");
    EXPECT_FALSE(state.knows_below(L"a"));
}

TEST_F(SyncStateTest, SizeIndexFollowsRecordAndForget)  // [SyncStateTests.md#SizeIndexFollowsRecordAndForget]
{
    SyncState state = filled_state();
    state.record(L"twin.txt", state_at(1234, 1, 2));
    EXPECT_EQ(state.with_size(1234).size(), 2u);
    EXPECT_EQ(state.with_size(0).size(), 1u);
    EXPECT_TRUE(state.with_size(77).empty());
    state.record(L"twin.txt", state_at(77, 1, 2));
    EXPECT_EQ(state.with_size(1234).size(), 1u);
    EXPECT_EQ(state.with_size(77), std::vector<wstring>{L"twin.txt"});
    state.forget(L"twin.txt");
    EXPECT_TRUE(state.with_size(77).empty());
    ASSERT_TRUE(write_sync_state(state_file(), filled_state()));
    SyncState unseen = *read();
    EXPECT_EQ(unseen.with_size(1234), std::vector<wstring>{L"Grüße/Äpfel.txt"}) << "a read state is indexed too";
    unseen.seen(L"Фото/Київ.jpg");
    unseen.forget_unseen();
    EXPECT_TRUE(unseen.with_size(1234).empty());
    EXPECT_EQ(unseen.with_size(0).size(), 1u);
}

TEST_F(SyncStateTest, PassNotebookHandsOutWhatWasPutOff)  // [SyncStateTests.md#PassNotebookHandsOutWhatWasPutOff]
{
    SyncState state(source_, target_);
    state.defer(PendingDeletion{L"a.txt", L"/s/a.txt", L"/t/a.txt", true, 10});
    state.defer(PendingDeletion{L"b.txt", L"/s/b.txt", L"/t/b.txt", false, 20});
    state.defer(PendingDirRemoval{L"/t/sub", false});
    EXPECT_FALSE(state.claim(L"c.txt").has_value());
    const std::optional<PendingDeletion> claimed = state.claim(L"a.txt");
    ASSERT_TRUE(claimed.has_value());
    EXPECT_EQ(claimed->target, L"/t/a.txt");
    EXPECT_FALSE(state.claim(L"a.txt").has_value());
    state.renamed(L"b.txt");
    state.defer(PendingDeletion{L"b.txt", L"/s/b.txt", L"/t/b.txt", false, 20});
    const std::vector<PendingDeletion> deletions = state.take_deletions();
    ASSERT_EQ(deletions.size(), 1u);
    EXPECT_EQ(deletions[0].key, L"b.txt");
    EXPECT_TRUE(state.take_deletions().empty());
    EXPECT_EQ(state.take_dir_removals().size(), 1u);
    EXPECT_TRUE(state.take_dir_removals().empty());
    EXPECT_FALSE(state.changed()) << "the notebook is not part of the stored state";
}
