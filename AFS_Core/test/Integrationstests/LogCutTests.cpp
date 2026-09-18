#include "pch.h"
#include <Logging.h>
#include <LogCut.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace AFS_Core;

namespace fs = std::filesystem;

namespace {
std::string local_timestamp(std::chrono::system_clock::time_point tp)  // [LogCutTests.md#local_timestamp]
{
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&t);
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return os.str();
}
}

TEST(LogCutTests, KeepsRecentLinesAfterExpiredOnes)  // [LogCutTests.md#KeepsRecentLinesAfterExpiredOnes]
{
    const fs::path log_path = fs::temp_directory_path() / ("autofilesync_logcut_" + std::to_string(getpid()) + ".log");
    fs::remove(log_path);
    const std::string recent = local_timestamp(std::chrono::system_clock::now() - std::chrono::hours(1));
    {
        std::ofstream out(log_path);
        out << "2020-01-01 10:00:00.000 Error\tancient entry\n";
        out << "ancient continuation without timestamp\n";
        out << recent << ".000 Info\trecent entry\n";
        out << "recent continuation without timestamp\n";
    }
    const std::wstring previous_log_path = Logging::instance().get_log_file_path_name();
    Logging::instance().set_log_file_path_name(log_path.wstring());
    Logging::instance().retention_days(7);
    LogCut()();

    std::string result;
    {
        std::ifstream in(log_path);  // [LogCutTests.md#own-block-for-windows]
        std::stringstream content;
        content << in.rdbuf();
        result = content.str();
    }
    EXPECT_EQ(result.find("ancient"), std::string::npos);
    EXPECT_NE(result.find("recent entry"), std::string::npos);
    EXPECT_NE(result.find("recent continuation"), std::string::npos);

    if (!previous_log_path.empty())
        Logging::instance().set_log_file_path_name(previous_log_path);
    fs::remove(log_path);
}

namespace {

class DailyLogCutTest : public ::testing::Test {  // [LogCutTests.md#DailyLogCutTest]
protected:
    const fs::path log_path_ = fs::temp_directory_path() / ("autofilesync_dailylogcut_" + std::to_string(getpid()) + ".log");
    std::wstring previous_log_path_;
    const std::chrono::system_clock::time_point start_ = std::chrono::system_clock::now();

    void SetUp() override
    {
        previous_log_path_ = Logging::instance().get_log_file_path_name();
        fs::remove(log_path_);
        Logging::instance().set_log_file_path_name(log_path_.wstring());
        Logging::instance().retention_days(7);
    }

    void TearDown() override
    {
        Logging::instance().set_log_file_path_name(previous_log_path_);
        fs::remove(log_path_);
    }

    void append_expired_line() const
    {
        Logging::instance().close_file();
        std::ofstream(log_path_, std::ios::app) << "2020-01-01 10:00:00.000 D Error\tancient entry\n";
        Logging::instance().open_file();
    }

    bool holds_expired_line() const
    {
        std::stringstream content;
        content << std::ifstream(log_path_).rdbuf();
        return content.str().find("ancient entry") != std::string::npos;
    }
};

} // namespace

TEST_F(DailyLogCutTest, CutsAtTheFirstCall)
{
    append_expired_line();
    DailyLogCut cut;
    cut(start_);
    EXPECT_FALSE(holds_expired_line());
}

TEST_F(DailyLogCutTest, DoesNotCutAgainWithinTheSameDay)
{
    DailyLogCut cut;
    cut(start_);
    append_expired_line();
    cut(start_ + std::chrono::hours(23));
    EXPECT_TRUE(holds_expired_line());
}

TEST_F(DailyLogCutTest, CutsAgainAfterOneDay)
{
    DailyLogCut cut;
    cut(start_);
    append_expired_line();
    cut(start_ + std::chrono::hours(24));
    EXPECT_FALSE(holds_expired_line());
}
