#include "pch.h"
#include "LoggingTests.h"
#include "Logging.h"
#include "SyncFile.h"
#include "Synchronize.h"
#include <fstream>

using namespace AFS_Core;

using namespace LoggingTests;

size_t countLogEntries(const std::wstring& searchText)
{
    std::wifstream logFile(Logging::instance().get_log_file_path_name());
    if (!logFile.is_open())
        return 0;
    std::wstring line;
    size_t count = 0;
    while (std::getline(logFile, line)) {
        if (line.find(searchText) != std::wstring::npos) {
            count++;
        }
    }
    logFile.close();
    return count;
}

TEST(LoggingTests, testNotMounted)
{	
    Synchronize::create(SyncType::Single);
    Synchronize::instance().start_logging(LogLevel::Debug, LoggingPath);
    Logging::instance().erase_file();
    SyncItem si;
    si.source(NotMountedSourcePathName);
    si.target(TargetDir);
    si.sync_cycle(0);
    
    const int testIterations = 3;
    Config::instance().reset();
	Config::instance().add(si, true);
    for (int i = 0; i < testIterations; ++i) {
        Synchronize::instance().operator()();
    }
    
    size_t logEntryCount = countLogEntries(L"Quellverzeichnis nicht erreichbar");  // [LoggingTests.md#single-log-entry]
    
    EXPECT_EQ(logEntryCount, 1);  // only one entry, even though the test ran repeatedly
}

namespace {

class LogOriginTest : public ::testing::Test {  // [LoggingTests.md#LogOriginTest]
protected:
    const std::filesystem::path log_path_ = std::filesystem::temp_directory_path() / ("autofilesync_logorigin_" + std::to_string(getpid()) + ".log");
    std::wstring previous_log_path_;
    LogLevel previous_level_ = LogLevel::Info;

    void SetUp() override
    {
        previous_log_path_ = Logging::instance().get_log_file_path_name();
        previous_level_ = Logging::instance().get_log_level();
        std::filesystem::remove(log_path_);
        Logging::instance().set_log_file_path_name(log_path_.wstring());
        Logging::instance().set_log_level(LogLevel::Info);
    }

    void TearDown() override
    {
        Logging::instance().origin(LogOrigin::Gui);
        Logging::instance().set_log_level(previous_level_);
        Logging::instance().set_log_file_path_name(previous_log_path_);
        std::filesystem::remove(log_path_);
    }

    std::string line_with(const std::string& text) const
    {
        std::ifstream log(log_path_);
        for (std::string line; std::getline(log, line);)
            if (line.find(text) != std::string::npos)
                return line;
        return {};
    }
};

constexpr std::size_t TIMESTAMP_LENGTH = 23;  // "2026-09-18 09:26:23.138"

} // namespace

TEST_F(LogOriginTest, GuiIsTheDefaultOrigin)
{
    log_info(L"message of the gui");
    EXPECT_EQ(line_with("message of the gui").substr(TIMESTAMP_LENGTH), " G Info\tmessage of the gui");
}

TEST_F(LogOriginTest, DaemonMarksItsLinesWithD)
{
    Logging::instance().origin(LogOrigin::Daemon);
    log_error(L"message of the daemon");
    EXPECT_EQ(line_with("message of the daemon").substr(TIMESTAMP_LENGTH), " D Error\tmessage of the daemon");
}
