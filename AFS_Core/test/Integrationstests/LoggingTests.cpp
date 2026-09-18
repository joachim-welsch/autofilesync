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
