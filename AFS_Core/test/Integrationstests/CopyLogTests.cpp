#include "pch.h"
#include "CopyLog.h"
#include "Synchronize.h"
#include "Config.h"
#include "Translation.h"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace AFS_Core;

namespace fs = std::filesystem;
namespace pt = boost::property_tree;
using std::wstring;

namespace {
    const wstring ENTRIES_KEY = L"Sicherungsnachweise";

    pt::wptree read_proof(const fs::path& file)  // [CopyLogTests.md#read_proof]
    {
        pt::wptree root;
        std::wifstream in(file);
        pt::read_json(in, root);
        return root;
    }

    std::vector<pt::wptree> entries_of(const fs::path& file)
    {
        std::vector<pt::wptree> retval;
        const pt::wptree root = read_proof(file);  // [CopyLogTests.md#named-root]
        for (const auto& entry : root.get_child(ENTRIES_KEY))
            retval.push_back(entry.second);
        return retval;
    }
}

class CopyLogTest : public ::testing::Test {
protected:
    fs::path test_dir_;
    fs::path proof_dir_;
    fs::path source_;
    fs::path target_;

    void SetUp() override
    {
        test_dir_ = fs::temp_directory_path() / ("autofilesync_backupproof_" + std::to_string(getpid()));
        fs::remove_all(test_dir_);
        proof_dir_ = test_dir_ / "nachweise";
        source_ = test_dir_ / "quelle";
        target_ = test_dir_ / "ziel";
        fs::create_directories(source_);
        fs::create_directories(target_);
        Config::instance().reset();
        Config::instance().settings().backup_proof_path(proof_dir_.wstring());
    }

    void TearDown() override
    {
        Config::instance().reset();
        fs::remove_all(test_dir_);
    }

    void write_source_file(const std::string& name, const std::string& content)
    {
        std::ofstream(source_ / name) << content;
    }

    SyncItem proven_item(const wstring& name = L"sicherung")
    {
        SyncItem item(name, SyncItemType::Dir, source_.wstring(), target_.wstring(), L"");
        item.backup_proof(true);
        return item;
    }

    void run_one_sync()  // one full pass of the real engine over whatever Config currently holds
    {
        Synchronize::create(SyncType::Single);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        Synchronize::destroy();
    }
};

TEST_F(CopyLogTest, SyncWithProofEnabledWritesEntryWithAllFields)
{
    write_source_file("a.txt", "inhalt");
    Config::instance().add(proven_item());

    run_one_sync();

    const fs::path file = copy_log_file_path(proof_dir_, L"sicherung");
    ASSERT_TRUE(fs::exists(file));
    const auto entries = entries_of(file);
    ASSERT_EQ(entries.size(), 1u);
    const pt::wptree& entry = entries[0];
    EXPECT_EQ(entry.get<wstring>(L"Name"), L"sicherung");
    EXPECT_EQ(entry.get<wstring>(L"Quelle"), source_.wstring());
    EXPECT_EQ(entry.get<wstring>(L"Ziel"), target_.wstring());
    EXPECT_EQ(entry.get<wstring>(L"Dateien"), L"1");
    EXPECT_EQ(entry.get<wstring>(L"Datenvolumen"), std::to_wstring(fs::file_size(source_ / "a.txt")));
    EXPECT_EQ(entry.get<wstring>(L"Status"), L"Erfolgreich");
    EXPECT_EQ(entry.get<wstring>(L"Zeitstempel").size(), 19u);  // only the shape of "YYYY-MM-TT hh:mm:ss" is deterministic here
    EXPECT_FALSE(entry.get_child_optional(L"Fehler").has_value());
}

TEST_F(CopyLogTest, PassWithoutActivityWritesNoEntry)  // [CopyLogTests.md#PassWithoutActivityWritesNoEntry]
{
    write_source_file("a.txt", "inhalt");
    Config::instance().add(proven_item());
    run_one_sync();
    const fs::path file = copy_log_file_path(proof_dir_, L"sicherung");
    ASSERT_EQ(entries_of(file).size(), 1u);

    Config::instance().reset();  // everything is in sync now, so this pass has nothing left to copy
    Config::instance().settings().backup_proof_path(proof_dir_.wstring());
    Config::instance().add(proven_item());
    run_one_sync();

    EXPECT_EQ(entries_of(file).size(), 1u);
}

TEST_F(CopyLogTest, FurtherActivityIsAppendedToTheSameFile)
{
    write_source_file("a.txt", "inhalt");
    Config::instance().add(proven_item());
    run_one_sync();

    write_source_file("b.txt", "mehr inhalt");
    Config::instance().reset();
    Config::instance().settings().backup_proof_path(proof_dir_.wstring());
    Config::instance().add(proven_item());
    run_one_sync();

    const auto entries = entries_of(copy_log_file_path(proof_dir_, L"sicherung"));
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].get<wstring>(L"Dateien"), L"1");
    EXPECT_EQ(entries[1].get<wstring>(L"Dateien"), L"1");
}

TEST_F(CopyLogTest, ItemWithoutProofFlagWritesNothing)
{
    write_source_file("a.txt", "inhalt");
    Config::instance().add(SyncItem(L"ohne", SyncItemType::Dir, source_.wstring(), target_.wstring(), L""));

    run_one_sync();

    EXPECT_FALSE(fs::exists(proof_dir_));
    EXPECT_TRUE(fs::exists(target_ / "a.txt"));  // the sync itself must still have happened - the flag only governs the proof
}

TEST_F(CopyLogTest, WithoutConfiguredPathNothingIsWritten)
{
    Config::instance().settings().backup_proof_path(L"");
    write_source_file("a.txt", "inhalt");
    Config::instance().add(proven_item());

    run_one_sync();

    EXPECT_FALSE(fs::exists(proof_dir_));
    EXPECT_TRUE(fs::exists(target_ / "a.txt"));
}

TEST_F(CopyLogTest, FailedRunIsRecordedAsFehlgeschlagenWithMessages)
{
    SyncReport report;
    report.error(L"Kopieren fehlgeschlagen: a.txt");
    report.error(L"Zieldatei nicht loeschbar: b.txt");

    ASSERT_TRUE(write_copy_log(proof_dir_, proven_item(), 0, report));

    const auto entries = entries_of(copy_log_file_path(proof_dir_, L"sicherung"));
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].get<wstring>(L"Status"), L"Fehlgeschlagen");
    std::vector<wstring> messages;
    for (const auto& error : entries[0].get_child(L"Fehler"))
        messages.push_back(error.second.data());
    ASSERT_EQ(messages.size(), 2u);
    EXPECT_EQ(messages[0], L"Kopieren fehlgeschlagen: a.txt");
    EXPECT_EQ(messages[1], L"Zieldatei nicht loeschbar: b.txt");
}

TEST_F(CopyLogTest, SyncFailureEndsUpInTheProof)  // [CopyLogTests.md#SyncFailureEndsUpInTheProof]
{
    const fs::path missing = test_dir_ / "nicht_vorhanden";
    SyncItem item(L"fehlerhaft", SyncItemType::Dir, missing.wstring(), target_.wstring(), L"");
    item.backup_proof(true);
    Config::instance().add(item);

    run_one_sync();

    const fs::path file = copy_log_file_path(proof_dir_, L"fehlerhaft");
    ASSERT_TRUE(fs::exists(file));
    const auto entries = entries_of(file);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].get<wstring>(L"Status"), L"Fehlgeschlagen");
    EXPECT_FALSE(entries[0].get_child(L"Fehler").empty());
}

TEST_F(CopyLogTest, UnreachableSourceIsProvenOnceNotOncePerPass)  // [CopyLogTests.md#UnreachableSourceIsProvenOnceNotOncePerPass]
{
    SyncItem item(L"fehlerhaft", SyncItemType::Dir, (test_dir_ / "nicht_vorhanden").wstring(),
        target_.wstring(), L"");
    item.backup_proof(true);
    item.sync_cycle(0);
    Config::instance().add(item);

    Synchronize* sync = Synchronize::create(SyncType::Continuous);
    ASSERT_TRUE(sync != nullptr);
    for (int i = 0; i < 80 && sync->loop_iterations() < 3; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const int iterations = sync->loop_iterations();
    Synchronize::destroy();
    ASSERT_GE(iterations, 3) << "Zu wenige Durchlaeufe - die Aussage des Tests traegt nicht";

    EXPECT_EQ(entries_of(copy_log_file_path(proof_dir_, L"fehlerhaft")).size(), 1u);
}

TEST_F(CopyLogTest, ItemNameWithPathCharactersBecomesAUsableFileName)
{
    SyncReport report;
    const wstring name = L"C:\\Daten -> Backup?";

    ASSERT_TRUE(write_copy_log(proof_dir_, proven_item(name), 1, report));

    const fs::path file = copy_log_file_path(proof_dir_, name);
    EXPECT_EQ(file.parent_path(), proof_dir_);
    ASSERT_TRUE(fs::exists(file));
    EXPECT_EQ(entries_of(file)[0].get<wstring>(L"Name"), name);  // [CopyLogTests.md#name-survives-sanitising]
}

TEST_F(CopyLogTest, UnreadableProofFileIsNotOverwritten)  // [CopyLogTests.md#UnreadableProofFileIsNotOverwritten]
{
    fs::create_directories(proof_dir_);
    const fs::path file = copy_log_file_path(proof_dir_, L"sicherung");
    const std::string content = "{ dies ist kein gueltiges json";
    std::ofstream(file) << content;

    SyncReport report;
    EXPECT_FALSE(write_copy_log(proof_dir_, proven_item(), 1, report));

    std::ifstream in(file);
    std::string stored((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(stored, content);
}

TEST_F(CopyLogTest, ErrorListIsCappedInsteadOfGrowingWithoutLimit)
{
    SyncReport report;
    for (std::size_t i = 0; i < SyncReport::MAX_ERRORS + 50; ++i)
        report.error(L"Fehler " + std::to_wstring(i));

    ASSERT_EQ(report.errors().size(), SyncReport::MAX_ERRORS + 1);
    EXPECT_EQ(report.errors().back(), translation::translate("error_list_capped"));  // [CopyLogTests.md#capped-list-marker]
}

TEST_F(CopyLogTest, ReportsOnlyReachTheGuardedRun)
{
    SyncReport report;
    {
        SyncReportGuard guard(&report);
        report_sync_volume(1024);
        report_sync_error(L"drin");
    }
    report_sync_volume(4096);
    report_sync_error(L"draussen");

    EXPECT_EQ(report.bytes_transferd(), 1024u);
    ASSERT_EQ(report.errors().size(), 1u);
    EXPECT_EQ(report.errors()[0], L"drin");
}
