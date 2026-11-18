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
#ifdef _UNIX
#include "Config.h"
#include "CopyLog.h"
#include "FileUtil.h"
#include "SshHostKeyVerification.h"
#include "SshTest.h"
#include "SyncFile.h"
#include "SyncFileSsh.h"
#include "SyncItem.h"
#include "SyncLink.h"
#include "SyncRunner.h"
#include "Synchronize.h"
#include "TestPaths.h"
#include "StrUtil.h"
#include "Translation.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <thread>
#include <pwd.h>
#include <sys/stat.h>

using namespace AFS_Core;
using namespace SSHTest;

namespace {

namespace fs = std::filesystem;
using std::chrono::seconds;

const std::string PLAIN_ACCOUNT = "afs_failure_plain";
const std::string PINNED_ACCOUNT = "afs_failure_pinned";  // [SyncSshFailureTests.md#pinned-account]

struct Outcome {
    std::size_t copies;
    SyncReport report;
};

class SyncSshFailureTest : public ::testing::Test {  // [SyncSshFailureTests.md#SyncSshFailureTest]
protected:
    const fs::path root_ = fs::path(tu::test_temp_dir()) / ("ssh_failure_" + std::to_string(TestPaths::process_id()));
    const fs::path local_dir_ = root_ / "lokal";
    const fs::path server_dir_ = root_ / "server";

    void SetUp() override
    {
        SKIP_WITHOUT_SSH_SERVER();
        fs::remove_all(root_);
        fs::create_directories(local_dir_);
        fs::create_directories(server_dir_);
        fs::permissions(root_, fs::perms::owner_all | fs::perms::group_exec | fs::perms::others_exec);
        Config::instance().add(account(PLAIN_ACCOUNT, ""));
        std::string error;
        const auto host_key = fetch_ssh_host_key(account("", ""), error);
        ASSERT_TRUE(host_key.has_value()) << error;
        Config::instance().add(account(PINNED_ACCOUNT, *host_key));
        Synchronize::start();
    }

    void TearDown() override
    {
        for (const std::string& name : {PLAIN_ACCOUNT, PINNED_ACCOUNT})
            remove_account(name);
        set_copy_hooks_for_test(nullptr, nullptr);
        std::error_code ec;
        fs::permissions(server_dir_, fs::perms::owner_all, ec);
        fs::remove_all(root_, ec);
    }

    static SshAccount account(const std::string& name, const std::string& host_key)
    {
        SshAccount account;
        account.set_name(name);
        account.set_user(SSH_USER);
        account.set_host(SSH_HOST);
        account.set_password(SSH_PASSWORD);
        account.set_host_key(host_key);
        return account;
    }

    static void remove_account(const std::string& name)
    {
        const auto accounts = Config::instance().ssh_accounts_snapshot();
        for (std::size_t i = accounts.size(); i-- > 0;)
            if (accounts[i].name() == name)
                Config::instance().remove_ssh_account(i);
    }

    std::wstring on_server(const std::string& account_name) const
    {
        return util::string_to_wstring(account_name + ":" + server_dir_.string());
    }

    static std::optional<fs::path> read_only_mount()  // [SyncSshFailureTests.md#read_only_mount]
    {
        std::ifstream mounts("/proc/self/mounts");
        std::string device, directory, type, options;
        for (std::string rest; mounts >> device >> directory >> type >> options && std::getline(mounts, rest);) {
            const bool read_only = options == "ro" || options.rfind("ro,", 0) == 0;
            if (read_only && directory.find('\\') == std::string::npos && first_file_in(directory))
                return directory;
        }
        return std::nullopt;
    }

    static std::optional<fs::path> first_file_in(const fs::path& directory)
    {
        std::error_code ec;
        for (fs::directory_iterator entry(directory, ec); !ec && entry != fs::directory_iterator(); entry.increment(ec))
            if (entry->is_regular_file(ec) && (entry->status(ec).permissions() & fs::perms::others_read) != fs::perms::none)
                return entry->path();
        return std::nullopt;
    }

    static bool owned_by_another_than_the_ssh_user(const fs::path& file)  // [SyncSshFailureTests.md#TimeAndRightsTheServerWouldNotSetAreReportedAndTheCopyStays]
    {
        struct stat info {};
        const passwd* ssh_user = getpwnam(SSH_USER);
        return ssh_user != nullptr && stat(file.c_str(), &info) == 0 && info.st_uid != ssh_user->pw_uid;
    }

    static std::set<std::string> names_in(const fs::path& dir)
    {
        std::set<std::string> names;
        for (const fs::directory_entry& entry : fs::directory_iterator(dir))
            names.insert(entry.path().filename().string());
        return names;
    }

    static Outcome run(const SyncItem& si)  // [LocalPermissionTests.md#run]
    {
        Outcome outcome;
        SyncReportGuard guard(&outcome.report);
        outcome.copies = tu::run_sync(si);
        return outcome;
    }
};

} // namespace

TEST_F(SyncSshFailureTest, UnlistableRemoteSourceDirIsReportedAndDeletesNothingItStillHas)  // [SyncSshFailureTests.md#UnlistableRemoteSourceDirIsReportedAndDeletesNothingItStillHas]
{
    tu::write_text_file(server_dir_ / "a.txt", "auf beiden Seiten");
    tu::write_text_file(server_dir_ / "neu.txt", "nur auf dem Server");
    tu::write_text_file(local_dir_ / "a.txt", "auf beiden Seiten");
    fs::permissions(server_dir_, fs::perms::owner_all | fs::perms::group_exec | fs::perms::others_exec);  // [SyncSshFailureTests.md#listing-refused]
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::Dir);
    si.source(on_server(PLAIN_ACCOUNT));
    si.target(local_dir_.wstring());
    si.delete_in_target(true);

    const Outcome outcome = run(si);

    EXPECT_TRUE(fs::exists(local_dir_ / "a.txt")) << "eine nicht auflistbare Quelle ist keine leere Quelle";
    EXPECT_FALSE(fs::exists(local_dir_ / "neu.txt"));
    EXPECT_FALSE(outcome.report.successful()) << "ein Lauf, der die Quelle nicht auflisten konnte, darf nicht als sauber gelten";
}

TEST_F(SyncSshFailureTest, PartFileThatCannotReplaceTheTargetLeavesTheOldFile)  // [SyncSshFailureTests.md#PartFileThatCannotReplaceTheTargetLeavesTheOldFile]
{
    tu::write_text_file(local_dir_ / "a.txt", "neu");
    tu::write_text_file(server_dir_ / "a.txt", "alt", seconds(3600));
    fs::permissions(server_dir_, fs::perms::all | fs::perms::sticky_bit);  // [SyncSshFailureTests.md#sticky-directory]
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::File);
    si.source((local_dir_ / "a.txt").wstring());
    si.target(on_server(PINNED_ACCOUNT) + L"/a.txt");

    const Outcome outcome = run(si);

    EXPECT_EQ(outcome.copies, 0u);
    EXPECT_FALSE(outcome.report.successful());
    EXPECT_EQ(tu::read_text_file(server_dir_ / "a.txt"), "alt") << "die alte Datei muss bleiben, wenn die neue sie nicht ersetzen kann";
    EXPECT_FALSE(fs::exists(server_dir_ / ("a.txt" + util::wstring_to_string(PART_SUFFIX)))) << "die Teildatei darf nicht liegen bleiben";
}

TEST_F(SyncSshFailureTest, RemoteFileAndDirectoryThatCannotBeRemovedNameTheirReason)  // [SyncSshFailureTests.md#RemoteFileAndDirectoryThatCannotBeRemovedNameTheirReason]
{
    tu::write_text_file(server_dir_ / "a.txt", "nur auf dem Server");
    fs::create_directories(server_dir_ / "leer");
    fs::permissions(server_dir_, fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec);
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::File);
    si.source((local_dir_ / "a.txt").wstring());
    si.target(on_server(PLAIN_ACCOUNT) + L"/a.txt");
    si.delete_in_target(true);

    const Outcome outcome = run(si);

    ASSERT_EQ(outcome.report.errors().size(), 1u);
    const std::wstring& error = outcome.report.errors().front();
    EXPECT_NE(error.find(translation::translate("target_delete_failed") + on_server(PLAIN_ACCOUNT) + L"/a.txt"), std::wstring::npos) << error;
    EXPECT_NE(error.find(translation::translate("permission_denied")), std::wstring::npos) << error;
    EXPECT_TRUE(fs::exists(server_dir_ / "a.txt"));
    EXPECT_EQ(ssh_rmdir_refusal(on_server(PLAIN_ACCOUNT) + L"/leer"), fsu::FileInfoResult::PermissionDenied);
    EXPECT_TRUE(fs::exists(server_dir_ / "leer"));
}

TEST_F(SyncSshFailureTest, TimeAndRightsTheServerWouldNotSetAreReportedAndTheCopyStays)  // [SyncSshFailureTests.md#TimeAndRightsTheServerWouldNotSetAreReportedAndTheCopyStays]
{
    tu::write_text_file(local_dir_ / "a.txt", "neuer Inhalt", seconds(7200));
    tu::write_text_file(server_dir_ / "a.txt", "alt");
    fs::permissions(server_dir_ / "a.txt", fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::group_write | fs::perms::others_read | fs::perms::others_write);
    if (!owned_by_another_than_the_ssh_user(server_dir_ / "a.txt"))
        GTEST_SKIP() << "der SSH-Testbenutzer ist der Besitzer der Datei und darf Zeit und Rechte setzen";
    const std::wstring target = on_server(PLAIN_ACCOUNT) + L"/a.txt";
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::File);
    si.source((local_dir_ / "a.txt").wstring());
    si.target(target);
    si.copy_when_target_newer(true);

    const Outcome outcome = run(si);

    EXPECT_EQ(outcome.copies, 1u) << "die Datei ist angekommen und zaehlt";
    EXPECT_EQ(tu::read_text_file(server_dir_ / "a.txt"), "neuer Inhalt") << "die Kopie wird nicht zurueckgenommen";
    ASSERT_EQ(outcome.report.errors().size(), 2u);
    EXPECT_EQ(outcome.report.errors()[0], translation::translate("file_time_not_settable") + target);
    EXPECT_EQ(outcome.report.errors()[1], fsu::make_error_message(fsu::FileInfoResult::CannotSetFileAttributes) + L": " + target);
}

TEST_F(SyncSshFailureTest, AnUploadWhoseTimeAndRightsWereSetReportsNothing)  // [SyncSshFailureTests.md#AnUploadWhoseTimeAndRightsWereSetReportsNothing]
{
    tu::write_text_file(local_dir_ / "a.txt", "neu", seconds(7200));
    fs::permissions(server_dir_, fs::perms::all);
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::File);
    si.source((local_dir_ / "a.txt").wstring());
    si.target(on_server(PLAIN_ACCOUNT) + L"/a.txt");

    const Outcome outcome = run(si);

    EXPECT_EQ(outcome.copies, 1u);
    EXPECT_TRUE(outcome.report.errors().empty()) << util::wstring_to_string(outcome.report.errors().empty() ? std::wstring() : outcome.report.errors().front());
}

TEST_F(SyncSshFailureTest, TheServerTellsTheRoomAFileHasAndCountsTheFileItReplaces)  // [SyncSshFailureTests.md#TheServerTellsTheRoomAFileHasAndCountsTheFileItReplaces]
{
    constexpr std::uint64_t OLD_BYTES = 32u << 20, TOLERANCE = 8u << 20;
    tu::write_large_file(server_dir_ / "alt.bin", OLD_BYTES);
    const std::wstring old_file = on_server(PLAIN_ACCOUNT) + L"/alt.bin";

    const std::optional<std::uint64_t> beside = ssh_room_for(on_server(PLAIN_ACCOUNT) + L"/neu.bin", false);
    const std::optional<std::uint64_t> replacing = ssh_room_for(old_file, false);
    const std::optional<std::uint64_t> keeping = ssh_room_for(old_file, true);
    const std::optional<std::uint64_t> pinned = ssh_room_for(on_server(PINNED_ACCOUNT) + L"/alt.bin", false);
    const std::uint64_t local = fs::space(server_dir_).available;

    ASSERT_TRUE(beside && replacing && keeping && pinned);
    const auto near = [&](std::uint64_t value, std::uint64_t expected) { return value + TOLERANCE > expected && value < expected + TOLERANCE; };
    EXPECT_TRUE(near(*beside, local)) << *beside << " statt " << local;
    EXPECT_TRUE(near(*replacing, local + OLD_BYTES)) << "die Datei, die ueberschrieben wird, gibt ihren Platz frei";
    EXPECT_TRUE(near(*keeping, local)) << "mit dem Generationenprinzip bleibt die alte Datei liegen";
    EXPECT_TRUE(near(*pinned, local)) << "eine Kopie ueber eine Teildatei braucht den Platz neben der alten";
}

TEST_F(SyncSshFailureTest, AnUploadTheServerHasNoRoomForIsNotTriedAndTheOldFileStays)  // [SyncSshFailureTests.md#AnUploadTheServerHasNoRoomForIsNotTriedAndTheOldFileStays]
{
    tu::write_text_file(local_dir_ / "a.txt", "neuer Inhalt, der nicht passt");
    tu::write_text_file(server_dir_ / "a.txt", "alt", seconds(3600));
    fs::permissions(server_dir_ / "a.txt", fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::group_write | fs::perms::others_read | fs::perms::others_write);
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::File);
    si.source((local_dir_ / "a.txt").wstring());
    si.target(on_server(PLAIN_ACCOUNT) + L"/a.txt");
    set_ssh_room_for_test(5);

    const Outcome outcome = run(si);
    set_ssh_room_for_test(std::nullopt);

    EXPECT_EQ(outcome.copies, 0u);
    EXPECT_EQ(tu::read_text_file(server_dir_ / "a.txt"), "alt") << "ein Upload, der nicht passt, darf die alte Datei nicht leeren";
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_NE(outcome.report.errors().front().find(fsu::make_error_message(fsu::FileInfoResult::RemoteStorageFull)), std::wstring::npos) << "voll ist der Server, nicht der eigene Datentraeger";
    EXPECT_FALSE(outcome.report.successful());
}

TEST_F(SyncSshFailureTest, ARefusedLoginIsNamedOnceAndEndsTheItem)  // [SyncSshFailureTests.md#ARefusedLoginIsNamedOnceAndEndsTheItem]
{
    SshAccount wrong = account("afs_failure_wrong", "");
    wrong.set_password("falsches-passwort");
    Config::instance().add(wrong);
    SyncReport report;
    {
        SyncReportGuard guard(&report);
        EXPECT_TRUE(ssh_list_dir(on_server("afs_failure_wrong")).empty());
        EXPECT_TRUE(sync_abandoned()) << "nach einer abgelehnten Anmeldung wird nichts weiter versucht";
    }
    remove_account("afs_failure_wrong");

    ASSERT_EQ(report.errors().size(), 1u);
    const std::wstring& error = report.errors().front();
    EXPECT_NE(error.find(fsu::make_error_message(fsu::FileInfoResult::AuthenticationFailed)), std::wstring::npos) << error;
    EXPECT_NE(error.find(translation::translate("remaining_files_skipped")), std::wstring::npos) << error;
}

TEST_F(SyncSshFailureTest, AHostThatCannotBeResolvedOrDoesNotListenIsToldApart)  // [SyncSshFailureTests.md#AHostThatCannotBeResolvedOrDoesNotListenIsToldApart]
{
    SshAccount unknown = account("afs_failure_unknown", "");
    unknown.set_host("afs-gibt-es-nicht.invalid");
    SshAccount closed = account("afs_failure_closed", "");
    closed.set_port(1);
    Config::instance().add(unknown);
    Config::instance().add(closed);

    const fsu::FileInfoResult no_name = ssh_remove_refusal(L"afs_failure_unknown:/tmp/a.txt");
    const fsu::FileInfoResult no_listener = ssh_remove_refusal(L"afs_failure_closed:/tmp/a.txt");
    SyncReport report;
    {
        SyncReportGuard guard(&report);
        EXPECT_TRUE(ssh_list_dir(L"afs_failure_closed:/tmp").empty());
        EXPECT_TRUE(sync_abandoned()) << "ein Verzeichnis, das sich nicht auflisten laesst, weil der Server fehlt, beendet das Element";
    }
    remove_account("afs_failure_unknown");
    remove_account("afs_failure_closed");

    EXPECT_EQ(no_name, fsu::FileInfoResult::NetworkUnavailable);
    EXPECT_EQ(no_listener, fsu::FileInfoResult::ServerUnreachable);
    ASSERT_EQ(report.errors().size(), 1u);
    EXPECT_NE(report.errors().front().find(fsu::make_error_message(fsu::FileInfoResult::ServerUnreachable)), std::wstring::npos);
}

TEST_F(SyncSshFailureTest, AnItemThatCannotBeReachedSaysWhy)  // [SyncSshFailureTests.md#AnItemThatCannotBeReachedSaysWhy]
{
    SshAccount unknown = account("afs_failure_unknown", "");
    unknown.set_host("afs-gibt-es-nicht.invalid");
    Config::instance().add(unknown);
    SyncItem si(L"ohne-namen", SyncItemType::Dir, local_dir_.wstring(), L"afs_failure_unknown:/tmp/afs-ziel", L"*");
    si.sync_cycle(0);
    Config::instance().add(si);

    Synchronize* sync = Synchronize::create(SyncType::Single);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    const std::vector<UnreachableItem> unreachable = sync->unreachable();
    Synchronize::destroy();
    Config::instance().erase_by_name(L"ohne-namen");
    remove_account("afs_failure_unknown");

    ASSERT_EQ(unreachable.size(), 1u);
    EXPECT_NE(unreachable.front().message.find(fsu::make_error_message(fsu::FileInfoResult::NetworkUnavailable)), std::wstring::npos) << unreachable.front().message;
}

TEST_F(SyncSshFailureTest, AnUploadTheServerRefusesNamesTheReason)  // [SyncSshFailureTests.md#AnUploadTheServerRefusesNamesTheReason]
{
    fs::permissions(server_dir_, fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec);
    if (!owned_by_another_than_the_ssh_user(server_dir_))
        GTEST_SKIP() << "der SSH-Testbenutzer ist der Besitzer des Verzeichnisses und darf darin schreiben";
    tu::write_text_file(local_dir_ / "a.txt", "a");
    tu::write_text_file(local_dir_ / "b.txt", "b");
    SyncItem si(L"kein-schreibrecht", SyncItemType::Dir, local_dir_.wstring(), on_server(PLAIN_ACCOUNT), L"*");
    si.sync_cycle(0);

    const Outcome outcome = run(si);

    ASSERT_EQ(outcome.report.errors().size(), 2u) << "fehlendes Schreibrecht gilt je Datei, der Lauf geht weiter";
    for (const std::wstring& error : outcome.report.errors()) {
        EXPECT_NE(error.find(translation::translate("copy_failed_for")), std::wstring::npos) << error;
        EXPECT_NE(error.find(fsu::make_error_message(fsu::FileInfoResult::PermissionDenied)), std::wstring::npos) << error;
    }
}

TEST_F(SyncSshFailureTest, AnUploadThatArrivedWithAnotherSizeIsAFailedCopy)  // [SyncSshFailureTests.md#AnUploadThatArrivedWithAnotherSizeIsAFailedCopy]
{
    tu::write_text_file(local_dir_ / "a.txt", "zwanzig Bytes Inhalt.");
    fs::permissions(server_dir_, fs::perms::all);
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::File);
    si.source((local_dir_ / "a.txt").wstring());
    si.target(on_server(PLAIN_ACCOUNT) + L"/a.txt");
    set_copy_hooks_for_test(nullptr, [&] { fs::remove(server_dir_ / "a.txt"); std::ofstream(server_dir_ / "a.txt", std::ios::binary) << "kurz"; });

    const Outcome outcome = run(si);
    set_copy_hooks_for_test(nullptr, nullptr);

    EXPECT_EQ(outcome.copies, 0u) << "bisher galt jede Uebertragung ohne Fehlermeldung als gelungen";
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_NE(outcome.report.errors().front().find(fsu::make_error_message(fsu::FileInfoResult::FileSizeMismatch)), std::wstring::npos) << outcome.report.errors().front();
    EXPECT_FALSE(fs::exists(server_dir_ / "a.txt")) << "die abweichende Datei wird wie eine halbe entfernt";
}

TEST_F(SyncSshFailureTest, ASourceChangedDuringAnUploadIsReportedAndTheCopyStays)  // [SyncSshFailureTests.md#ASourceChangedDuringAnUploadIsReportedAndTheCopyStays]
{
    tu::write_text_file(local_dir_ / "a.txt", "erster Stand", seconds(60));
    fs::permissions(server_dir_, fs::perms::all);
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::File);
    si.source((local_dir_ / "a.txt").wstring());
    si.target(on_server(PLAIN_ACCOUNT) + L"/a.txt");
    set_copy_hooks_for_test(nullptr, [&] { tu::write_text_file(local_dir_ / "a.txt", "zweiter, laengerer Stand"); });

    const Outcome outcome = run(si);
    set_copy_hooks_for_test(nullptr, nullptr);

    EXPECT_EQ(outcome.copies, 0u);
    ASSERT_EQ(outcome.report.errors().size(), 1u);
    EXPECT_NE(outcome.report.errors().front().find(fsu::make_error_message(fsu::FileInfoResult::SourceFileChanged)), std::wstring::npos) << outcome.report.errors().front();
    EXPECT_EQ(tu::read_text_file(server_dir_ / "a.txt"), "erster Stand") << "auf dem Server gibt es kein altes Ziel zum Zurueckholen: die Kopie bleibt";
}

TEST_F(SyncSshFailureTest, ALinkTheServerRefusesForLackOfRightsIsNotTakenForAServerWithoutLinks)  // [SyncSshFailureTests.md#ALinkTheServerRefusesForLackOfRightsIsNotTakenForAServerWithoutLinks]
{
    fs::create_directories(server_dir_ / "offen");
    fs::permissions(server_dir_ / "offen", fs::perms::all);
    fs::permissions(server_dir_, fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec);
    if (!owned_by_another_than_the_ssh_user(server_dir_))
        GTEST_SKIP() << "der SSH-Testbenutzer ist der Besitzer des Verzeichnisses und darf darin schreiben";

    const fsu::FileInfoResult refused = ssh_create_link(L"a.txt", on_server(PLAIN_ACCOUNT) + L"/link");
    const fsu::FileInfoResult made = ssh_create_link(L"a.txt", on_server(PLAIN_ACCOUNT) + L"/offen/link");

    EXPECT_EQ(refused, fsu::FileInfoResult::PermissionDenied) << "nicht LinksNotSupported: das waere nur ein Hinweis";
    EXPECT_EQ(names_in(server_dir_), (std::set<std::string>{"offen"})) << "die Probe darf nichts hinterlassen";
    EXPECT_EQ(made, fsu::FileInfoResult::OK);
    EXPECT_TRUE(fs::is_symlink(server_dir_ / "offen" / "link"));
    SSHTest::remove_remote_tree((server_dir_ / "offen").string());
}

TEST_F(SyncSshFailureTest, ALinkWhoseServerSideCannotBeAskedIsReported)  // [SyncSshFailureTests.md#ALinkWhoseServerSideCannotBeAskedIsReported]
{
    fs::create_directories(server_dir_ / "zu");
    fs::permissions(server_dir_ / "zu", fs::perms::owner_all);
    if (!owned_by_another_than_the_ssh_user(server_dir_ / "zu"))
        GTEST_SKIP() << "der SSH-Testbenutzer ist der Besitzer des Verzeichnisses und darf hineinsehen";
    tu::write_text_file(local_dir_ / "a.txt", "a");
    fs::create_symlink("a.txt", local_dir_ / "link");
    const std::wstring target = on_server(PLAIN_ACCOUNT) + L"/zu/link";
    SyncItem si;
    si.sync_cycle(0);
    si.sync_item_type(SyncItemType::File);
    si.source((local_dir_ / "link").wstring());
    si.target(target);
    si.symlink_mode(SymlinkMode::AsLink);
    std::size_t copies = 0;
    LinkTally tally;
    SyncReport report;
    {
        SyncReportGuard guard(&report);
        SyncLink(si, copies, false, tally, nullptr, L"", L"")();
    }

    ASSERT_EQ(report.errors().size(), 1u) << "bisher geschah hier nichts, auch keine Meldung";
    EXPECT_EQ(report.errors().front(), fsu::make_error_message(fsu::FileInfoResult::ErrorReadFileInfo) + L": " + target);
    EXPECT_EQ(copies, 0u);
}

TEST_F(SyncSshFailureTest, AReadOnlyFilesystemOfTheServerIsRecognizedAndNamedForARefusedRemoval)  // [SyncSshFailureTests.md#AReadOnlyFilesystemOfTheServerIsRecognizedAndNamedForARefusedRemoval]
{
    const std::optional<fs::path> mount = read_only_mount();
    if (!mount)
        GTEST_SKIP() << "auf diesem Rechner ist kein Dateisystem mit einer lesbaren Datei nur lesbar eingehaengt";
    const std::wstring remote_mount = util::string_to_wstring(PLAIN_ACCOUNT + ":" + mount->string());
    const fs::path file = *first_file_in(*mount);

    EXPECT_TRUE(ssh_read_only_filesystem(remote_mount));
    EXPECT_TRUE(ssh_read_only_filesystem(remote_mount + L"/afs-gibt-es-nicht/datei.txt")) << "gemessen am vorhandenen Teil des Pfads";
    EXPECT_FALSE(ssh_read_only_filesystem(on_server(PLAIN_ACCOUNT)));
    EXPECT_FALSE(ssh_read_only_filesystem(on_server(PLAIN_ACCOUNT) + L"/afs-gibt-es-nicht/datei.txt"));
    EXPECT_EQ(ssh_remove_refusal(util::string_to_wstring(PLAIN_ACCOUNT + ":" + file.string())), fsu::FileInfoResult::ReadOnlyFilesystem) << file.string();
    EXPECT_EQ(ssh_create_link(L"egal", remote_mount + L"/afs-link"), fsu::FileInfoResult::ReadOnlyFilesystem);
    EXPECT_TRUE(fs::exists(file));
}

TEST_F(SyncSshFailureTest, AReadOnlyTargetOnTheServerIsOneErrorOfThePass)  // [SyncSshFailureTests.md#AReadOnlyTargetOnTheServerIsOneErrorOfThePass]
{
    const std::optional<fs::path> mount = read_only_mount();
    if (!mount)
        GTEST_SKIP() << "auf diesem Rechner ist kein Dateisystem mit einer lesbaren Datei nur lesbar eingehaengt";
    for (const char* name : {"a.txt", "b.txt", "c.txt"})
        tu::write_text_file(local_dir_ / name, name);
    const std::wstring target = util::string_to_wstring(PLAIN_ACCOUNT + ":" + (*mount / "afs-gibt-es-nicht").string());
    SyncItem si(L"server-schreibschutz", SyncItemType::Dir, local_dir_.wstring(), target, L"*");
    si.sync_cycle(0);
    Config::instance().add(si);

    Synchronize::create(SyncType::Single);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    Synchronize::destroy();
    const std::optional<SyncItem> passed = Config::instance().find_by_name(L"server-schreibschutz");
    Config::instance().erase_by_name(L"server-schreibschutz");
    const std::optional<LastRun> run = passed ? passed->last_run() : std::nullopt;

    ASSERT_TRUE(run.has_value());
    EXPECT_EQ(run->errors, 1u);
    EXPECT_EQ(run->first_error, translation::translate("target_read_only") + target);
}

TEST_F(SyncSshFailureTest, ACopyRefusedByAReadOnlyServerNamesTheReasonAndAbandonsTheItem)  // [SyncSshFailureTests.md#ACopyRefusedByAReadOnlyServerNamesTheReasonAndAbandonsTheItem]
{
    const std::optional<fs::path> mount = read_only_mount();
    if (!mount)
        GTEST_SKIP() << "auf diesem Rechner ist kein Dateisystem mit einer lesbaren Datei nur lesbar eingehaengt";
    for (const char* name : {"afs-a.txt", "afs-b.txt", "afs-c.txt"})
        tu::write_text_file(local_dir_ / name, name);
    SyncItem si(L"server-schreibschutz", SyncItemType::Dir, local_dir_.wstring(), util::string_to_wstring(PLAIN_ACCOUNT + ":" + mount->string()), L"afs-*.txt");
    si.sync_cycle(0);
    set_ssh_room_for_test(std::uint64_t(1) << 40);

    const Outcome outcome = run(si);
    set_ssh_room_for_test(std::nullopt);

    ASSERT_EQ(outcome.report.errors().size(), 1u) << "die zweite und dritte Datei werden nicht mehr versucht";
    const std::wstring& error = outcome.report.errors().front();
    EXPECT_NE(error.find(translation::translate("copy_failed_for")), std::wstring::npos) << error;
    EXPECT_NE(error.find(translation::translate("filesystem_read_only")), std::wstring::npos) << error;
    EXPECT_NE(error.find(translation::translate("remaining_files_skipped")), std::wstring::npos) << error;
    EXPECT_EQ(outcome.copies, 0u);
}

TEST_F(SyncSshFailureTest, AReadOnlyServerWithoutRoomIsCalledReadOnlyAndNotFull)  // [SyncSshFailureTests.md#AReadOnlyServerWithoutRoomIsCalledReadOnlyAndNotFull]
{
    const std::optional<fs::path> mount = read_only_mount();
    if (!mount)
        GTEST_SKIP() << "auf diesem Rechner ist kein Dateisystem mit einer lesbaren Datei nur lesbar eingehaengt";
    for (const char* name : {"afs-a.txt", "afs-b.txt", "afs-c.txt"})
        tu::write_text_file(local_dir_ / name, name);
    SyncItem si(L"server-schreibschutz", SyncItemType::Dir, local_dir_.wstring(), util::string_to_wstring(PLAIN_ACCOUNT + ":" + mount->string()), L"afs-*.txt");
    si.sync_cycle(0);
    set_ssh_room_for_test(0);

    const Outcome outcome = run(si);
    set_ssh_room_for_test(std::nullopt);

    ASSERT_EQ(outcome.report.errors().size(), 1u);
    const std::wstring& error = outcome.report.errors().front();
    EXPECT_NE(error.find(translation::translate("filesystem_read_only")), std::wstring::npos) << error;
    EXPECT_NE(error.find(translation::translate("remaining_files_skipped")), std::wstring::npos) << error;
    EXPECT_EQ(error.find(fsu::make_error_message(fsu::FileInfoResult::DiskFull)), std::wstring::npos) << "Platz ist nicht, was diesem Laufwerk fehlt";
}

TEST_F(SyncSshFailureTest, ADirectoryRefusedByAReadOnlyServerNamesTheReason)  // [SyncSshFailureTests.md#ADirectoryRefusedByAReadOnlyServerNamesTheReason]
{
    const std::optional<fs::path> mount = read_only_mount();
    if (!mount)
        GTEST_SKIP() << "auf diesem Rechner ist kein Dateisystem mit einer lesbaren Datei nur lesbar eingehaengt";
    tu::write_text_file(local_dir_ / "a.txt", "a");
    const std::wstring target = util::string_to_wstring(PLAIN_ACCOUNT + ":" + (*mount / "afs-gibt-es-nicht").string());
    SyncItem si(L"server-schreibschutz", SyncItemType::Dir, local_dir_.wstring(), target, L"*");
    si.sync_cycle(0);

    const Outcome outcome = run(si);

    ASSERT_EQ(outcome.report.errors().size(), 1u);
    const std::wstring& error = outcome.report.errors().front();
    EXPECT_NE(error.find(translation::translate("ssh_target_dir_not_creatable") + target), std::wstring::npos) << error;
    EXPECT_NE(error.find(translation::translate("filesystem_read_only")), std::wstring::npos) << error;
}
#endif
