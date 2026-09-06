#include "pch.h"
#include "BackupProof.h"
#include "Synchronize.h"
#include "Config.h"
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
    const wstring entriesKey = L"Sicherungsnachweise";

    // The whole feature is about what ends up in the file, so every test reads
    // the file back rather than trusting the writer's return value.
    pt::wptree readProof(const fs::path& file)
    {
        pt::wptree root;
        std::wifstream in(file);
        pt::read_json(in, root);
        return root;
    }

    std::vector<pt::wptree> entriesOf(const fs::path& file)
    {
        std::vector<pt::wptree> retval;
        // Kept in a named variable: get_child() hands back a reference into the
        // tree, and a temporary tree would already be gone inside the loop.
        const pt::wptree root = readProof(file);
        for (const auto& entry : root.get_child(entriesKey))
            retval.push_back(entry.second);
        return retval;
    }
}

class BackupProofTest : public ::testing::Test {
protected:
    fs::path testDir_;
    fs::path proofDir_;
    fs::path source_;
    fs::path target_;

    void SetUp() override
    {
        testDir_ = fs::temp_directory_path() / ("autofilesync_backupproof_" + std::to_string(getpid()));
        fs::remove_all(testDir_);
        proofDir_ = testDir_ / "nachweise";
        source_ = testDir_ / "quelle";
        target_ = testDir_ / "ziel";
        fs::create_directories(source_);
        fs::create_directories(target_);
        Config::instance().reset();
        Config::instance().settings().backupProofPath(proofDir_.wstring());
    }

    void TearDown() override
    {
        Config::instance().reset();
        fs::remove_all(testDir_);
    }

    void writeSourceFile(const std::string& name, const std::string& content)
    {
        std::ofstream(source_ / name) << content;
    }

    SyncItem provenItem(const wstring& name = L"sicherung")
    {
        SyncItem item(name, SyncItemType::Dir, source_.wstring(), target_.wstring(), L"");
        item.backupProof(true);
        return item;
    }

    // One full pass of the real engine over whatever Config currently holds.
    void runOneSync()
    {
        Synchronize::create(SyncType::Single);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        Synchronize::destroy();
    }
};

TEST_F(BackupProofTest, SyncWithProofEnabledWritesEntryWithAllFields)
{
    writeSourceFile("a.txt", "inhalt");
    Config::instance().add(provenItem());

    runOneSync();

    const fs::path file = backupProofFilePath(proofDir_, L"sicherung");
    ASSERT_TRUE(fs::exists(file));
    const auto entries = entriesOf(file);
    ASSERT_EQ(entries.size(), 1u);
    const pt::wptree& entry = entries[0];
    EXPECT_EQ(entry.get<wstring>(L"Name"), L"sicherung");
    EXPECT_EQ(entry.get<wstring>(L"Quelle"), source_.wstring());
    EXPECT_EQ(entry.get<wstring>(L"Ziel"), target_.wstring());
    EXPECT_EQ(entry.get<wstring>(L"Dateien"), L"1");
    EXPECT_EQ(entry.get<wstring>(L"Datenvolumen"), std::to_wstring(fs::file_size(source_ / "a.txt")));
    EXPECT_EQ(entry.get<wstring>(L"Status"), L"Erfolgreich");
    // "YYYY-MM-TT hh:mm:ss" - only the shape is deterministic here.
    EXPECT_EQ(entry.get<wstring>(L"Zeitstempel").size(), 19u);
    EXPECT_FALSE(entry.get_child_optional(L"Fehler").has_value());
}

// A pass that found nothing to do proves nothing. With a sync cycle down to one
// second, writing an entry for it anyway would bury the passes that did
// something under thousands that did not.
TEST_F(BackupProofTest, PassWithoutActivityWritesNoEntry)
{
    writeSourceFile("a.txt", "inhalt");
    Config::instance().add(provenItem());
    runOneSync();
    const fs::path file = backupProofFilePath(proofDir_, L"sicherung");
    ASSERT_EQ(entriesOf(file).size(), 1u);

    // Everything is in sync now, so this pass has nothing left to copy.
    Config::instance().reset();
    Config::instance().settings().backupProofPath(proofDir_.wstring());
    Config::instance().add(provenItem());
    runOneSync();

    EXPECT_EQ(entriesOf(file).size(), 1u);
}

TEST_F(BackupProofTest, FurtherActivityIsAppendedToTheSameFile)
{
    writeSourceFile("a.txt", "inhalt");
    Config::instance().add(provenItem());
    runOneSync();

    writeSourceFile("b.txt", "mehr inhalt");
    Config::instance().reset();
    Config::instance().settings().backupProofPath(proofDir_.wstring());
    Config::instance().add(provenItem());
    runOneSync();

    const auto entries = entriesOf(backupProofFilePath(proofDir_, L"sicherung"));
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].get<wstring>(L"Dateien"), L"1");
    EXPECT_EQ(entries[1].get<wstring>(L"Dateien"), L"1");
}

TEST_F(BackupProofTest, ItemWithoutProofFlagWritesNothing)
{
    writeSourceFile("a.txt", "inhalt");
    Config::instance().add(SyncItem(L"ohne", SyncItemType::Dir, source_.wstring(), target_.wstring(), L""));

    runOneSync();

    EXPECT_FALSE(fs::exists(proofDir_));
    // The sync itself must still have happened - the flag only governs the proof.
    EXPECT_TRUE(fs::exists(target_ / "a.txt"));
}

TEST_F(BackupProofTest, WithoutConfiguredPathNothingIsWritten)
{
    Config::instance().settings().backupProofPath(L"");
    writeSourceFile("a.txt", "inhalt");
    Config::instance().add(provenItem());

    runOneSync();

    EXPECT_FALSE(fs::exists(proofDir_));
    EXPECT_TRUE(fs::exists(target_ / "a.txt"));
}

TEST_F(BackupProofTest, FailedRunIsRecordedAsFehlgeschlagenWithMessages)
{
    SyncReport report;
    report.error(L"Kopieren fehlgeschlagen: a.txt");
    report.error(L"Zieldatei nicht loeschbar: b.txt");

    ASSERT_TRUE(writeBackupProof(proofDir_, provenItem(), 0, report));

    const auto entries = entriesOf(backupProofFilePath(proofDir_, L"sicherung"));
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].get<wstring>(L"Status"), L"Fehlgeschlagen");
    std::vector<wstring> messages;
    for (const auto& error : entries[0].get_child(L"Fehler"))
        messages.push_back(error.second.data());
    ASSERT_EQ(messages.size(), 2u);
    EXPECT_EQ(messages[0], L"Kopieren fehlgeschlagen: a.txt");
    EXPECT_EQ(messages[1], L"Zieldatei nicht loeschbar: b.txt");
}

// An unreadable source is a failure the proof has to state, not one it may
// leave out - otherwise the file only ever documents what went well.
TEST_F(BackupProofTest, SyncFailureEndsUpInTheProof)
{
    const fs::path missing = testDir_ / "nicht_vorhanden";
    SyncItem item(L"fehlerhaft", SyncItemType::Dir, missing.wstring(), target_.wstring(), L"");
    item.backupProof(true);
    Config::instance().add(item);

    runOneSync();

    const fs::path file = backupProofFilePath(proofDir_, L"fehlerhaft");
    ASSERT_TRUE(fs::exists(file));
    const auto entries = entriesOf(file);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].get<wstring>(L"Status"), L"Fehlgeschlagen");
    EXPECT_FALSE(entries[0].get_child(L"Fehler").empty());
}

// A source that stays gone is reported when it goes, not on every pass that
// finds it still gone: with a cycle down to a second, the second reading would
// leave a proof file of thousands of identical entries per day.
TEST_F(BackupProofTest, UnreachableSourceIsProvenOnceNotOncePerPass)
{
    SyncItem item(L"fehlerhaft", SyncItemType::Dir, (testDir_ / "nicht_vorhanden").wstring(),
        target_.wstring(), L"");
    item.backupProof(true);
    item.syncCycle(0);
    Config::instance().add(item);

    Synchronize* sync = Synchronize::create(SyncType::Continuous);
    ASSERT_TRUE(sync != nullptr);
    for (int i = 0; i < 80 && sync->loopIterations() < 3; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const int iterations = sync->loopIterations();
    Synchronize::destroy();
    ASSERT_GE(iterations, 3) << "Zu wenige Durchlaeufe - die Aussage des Tests traegt nicht";

    EXPECT_EQ(entriesOf(backupProofFilePath(proofDir_, L"fehlerhaft")).size(), 1u);
}

TEST_F(BackupProofTest, ItemNameWithPathCharactersBecomesAUsableFileName)
{
    SyncReport report;
    const wstring name = L"C:\\Daten -> Backup?";

    ASSERT_TRUE(writeBackupProof(proofDir_, provenItem(name), 1, report));

    const fs::path file = backupProofFilePath(proofDir_, name);
    EXPECT_EQ(file.parent_path(), proofDir_);
    ASSERT_TRUE(fs::exists(file));
    // The unusable characters are gone from the file name, but the entry still
    // states which sync item it belongs to.
    EXPECT_EQ(entriesOf(file)[0].get<wstring>(L"Name"), name);
}

// The proof file is evidence: an entry that cannot be added must not cost the
// entries already there.
TEST_F(BackupProofTest, UnreadableProofFileIsNotOverwritten)
{
    fs::create_directories(proofDir_);
    const fs::path file = backupProofFilePath(proofDir_, L"sicherung");
    const std::string content = "{ dies ist kein gueltiges json";
    std::ofstream(file) << content;

    SyncReport report;
    EXPECT_FALSE(writeBackupProof(proofDir_, provenItem(), 1, report));

    std::ifstream in(file);
    std::string stored((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(stored, content);
}

TEST_F(BackupProofTest, ErrorListIsCappedInsteadOfGrowingWithoutLimit)
{
    SyncReport report;
    for (std::size_t i = 0; i < SyncReport::maxErrors + 50; ++i)
        report.error(L"Fehler " + std::to_wstring(i));

    ASSERT_EQ(report.errors().size(), SyncReport::maxErrors + 1);
    // The last entry says the list is incomplete, so a capped list is never
    // mistaken for the full account of what went wrong.
    EXPECT_EQ(report.errors().back(), L"Weitere Fehler wurden nicht aufgezeichnet.");
}

TEST_F(BackupProofTest, ReportsOnlyReachTheGuardedRun)
{
    SyncReport report;
    {
        SyncReportGuard guard(&report);
        reportSyncVolume(1024);
        reportSyncError(L"drin");
    }
    reportSyncVolume(4096);
    reportSyncError(L"draussen");

    EXPECT_EQ(report.bytesTransferd(), 1024u);
    ASSERT_EQ(report.errors().size(), 1u);
    EXPECT_EQ(report.errors()[0], L"drin");
}
