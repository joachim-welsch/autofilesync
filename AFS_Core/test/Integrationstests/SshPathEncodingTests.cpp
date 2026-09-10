#include "pch.h"
#include "SyncItem.h"
#include "SyncTree.h"
#include "Synchronize.h"
#include "CopyLog.h"
#include "Config.h"
#include "SshAccount.h"
#include "TestPaths.h"
#include <StrUtil.h>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace AFS_Core;

using util::wstring_to_string;
namespace fs = std::filesystem;

// Reproduces the sync abort logged as
//   Error  /home/welschj/Schreibtisch  mmserver:/home/welschj/  wstring_convert::to_bytes
// SyncItem::source()/target() and SshPath::path() rebuild an SSH path with
// std::wstring(bytes.begin(), bytes.end()). char is signed, so every byte of a
// non-ASCII UTF-8 sequence widens to a negative wchar_t (0xc3 -> 0xffffffc3),
// a value codecvt_utf8 cannot encode. The next wstring_to_string() of that path
// - and every log line does one - throws std::range_error("wstring_convert::to_bytes").
// SyncFile's own catch handler logs the target as well, so the exception escapes
// from there and takes the whole directory branch with it.
namespace {
    constexpr char ACCOUNT_NAME[] = "mmserver";
    // RFC 2606 reserves .invalid, so the name never resolves: the tests cannot
    // reach a real host or write to the user's known_hosts. The defect fires
    // before any connection attempt anyway.
    constexpr char UNREACHABLE_HOST[] = "autofilesync.invalid";
    const std::wstring REMOTE_DIR = L"mmserver:/tmp/autofilesync_test/ssh_target";
    const std::wstring LOCAL_SOURCE_DIR = TestPaths::scratch_dir() + L"/ssh_path_encoding_src";
}

class SshPathEncodingTest : public ::testing::Test {
    std::vector<std::string> added_accounts_;
protected:
    void add_account(const std::string& name)
    {
        SshAccount account;
        account.set_name(name);
        account.set_user("testuser");
        account.set_host(UNREACHABLE_HOST);
        account.set_password("testpassword");
        Config::instance().add(account);
        added_accounts_.push_back(name);
    }
    void SetUp() override { add_account(ACCOUNT_NAME); }
    // Config is a process-wide singleton: an account left behind would change
    // which paths the following tests resolve as SSH paths.
    void TearDown() override
    {
        for (const auto& name : added_accounts_) {
            const auto accounts = Config::instance().ssh_accounts_snapshot();
            for (std::size_t i = 0; i < accounts.size(); ++i)
                if (accounts[i].name() == name) {
                    Config::instance().remove_ssh_account(i);
                    break;
                }
        }
        added_accounts_.clear();
    }
};

TEST_F(SshPathEncodingTest, SshTargetWithUmlautStaysConvertible)
{
    const std::wstring target = REMOTE_DIR + L"/Bücher.txt";
    SyncItem item;
    item.target(target);
    ASSERT_TRUE(item.ssh_target());
    EXPECT_NO_THROW(wstring_to_string(item.target()));
    EXPECT_EQ(item.target(), target);
}

TEST_F(SshPathEncodingTest, SshSourceWithUmlautStaysConvertible)
{
    const std::wstring source = REMOTE_DIR + L"/Bücher.txt";
    SyncItem item;
    item.source(source);
    ASSERT_TRUE(item.ssh_source());
    EXPECT_NO_THROW(wstring_to_string(item.source()));
    EXPECT_EQ(item.source(), source);
}

// SshPath::path() widens the account name the same way, so an umlaut in the
// account name alone is enough - no matter what the remote path looks like.
TEST_F(SshPathEncodingTest, SshAccountNameWithUmlautStaysConvertible)
{
    add_account("Büro");
    const std::wstring target = L"Büro:/tmp/autofilesync_test/ssh_target";
    SyncItem item;
    item.target(target);
    ASSERT_TRUE(item.ssh_target());
    EXPECT_NO_THROW(wstring_to_string(item.target()));
    EXPECT_EQ(item.target(), target);
}

// The reported failure end to end: one file with an umlaut in its name makes
// SyncTree report the conversion error for the whole item instead of syncing.
TEST_F(SshPathEncodingTest, TreeSyncToSshTargetDoesNotAbortOnUmlautFilename)
{
    fs::remove_all(LOCAL_SOURCE_DIR);
    fs::create_directories(LOCAL_SOURCE_DIR);
    std::ofstream(fs::path(LOCAL_SOURCE_DIR) / "Bücher.txt") << "Inhalt";
    std::ofstream(fs::path(LOCAL_SOURCE_DIR) / "zzz_after.txt") << "Inhalt";

    Synchronize::start();
    SyncItem item;
    item.sync_cycle(0);
    item.sync_item_type(SyncItemType::Tree);
    item.source(LOCAL_SOURCE_DIR);
    item.target(REMOTE_DIR);

    SyncReport report;
    SyncReportGuard guard(&report);
    std::size_t copy_count = 0;
    SyncTree tree(item, copy_count, false);
    tree();

    for (const auto& error : report.errors())
        EXPECT_EQ(error.find(L"wstring_convert"), std::wstring::npos)
            << "Konvertierungsfehler bricht die Synchronisation ab: " << error;

    fs::remove_all(LOCAL_SOURCE_DIR);
}
