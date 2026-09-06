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
    std::wifstream logFile(Logging::instance().getLogFilePathName());
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
    // Log-Datei vor dem Test leeren
    Synchronize::create(SyncType::Single);
    Synchronize::instance().startLogging(LogLevel::Debug, LoggingPath);
    Logging::instance().eraseFile();
    // Test-Setup
    SyncItem si;
    si.source(NotMountedSourcePathName);
    si.target(TargetDir);
    si.syncCycle(0);
    
    // Test mehrfach ausf�hren
    const int testIterations = 3;
    Config::instance().reset();
	Config::instance().add(si, true);
    for (int i = 0; i < testIterations; ++i) {
        Synchronize::instance().operator()();
    }
    
    // Pr�fen, dass nur ein Log-Eintrag f�r das nicht gemountete Laufwerk existiert
    // Hier suchen wir nach einem charakteristischen Text, der beim �berpr�fen 
    // nicht gemounteter Laufwerke protokolliert wird
    size_t logEntryCount = countLogEntries(L"Quellverzeichnis nicht erreichbar");
    
    // Es sollte nur ein Log-Eintrag vorhanden sein, auch wenn der Test mehrfach ausgef�hrt wurde
    EXPECT_EQ(logEntryCount, 1);
}
