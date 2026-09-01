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

namespace fs = std::filesystem;

namespace {
// getTimeFromLine() parses via mktime (local time), so the fixture timestamp
// must be formatted in local time as well.
std::string localTimestamp(std::chrono::system_clock::time_point tp)
{
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&t);
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return os.str();
}
}

// Fehler 8 (round 4): cut() latched its "too old" flag on the first expired
// line and never reset it. Logs are chronological (oldest lines first), so as
// soon as the retention span was ever exceeded, the whole log - including all
// current entries - was discarded on every program start (MainWindow runs
// LogCut in its constructor). Expiry must be decided per timestamped line;
// timestamp-less continuation lines follow the preceding timestamped line.
TEST(LogCutTests, KeepsRecentLinesAfterExpiredOnes)
{
    const fs::path logPath = fs::temp_directory_path() / ("autofilesync_logcut_" + std::to_string(getpid()) + ".log");
    fs::remove(logPath);
    const std::string recent = localTimestamp(std::chrono::system_clock::now() - std::chrono::hours(1));
    {
        std::ofstream out(logPath);
        out << "2020-01-01 10:00:00.000 Error\tancient entry\n";
        out << "ancient continuation without timestamp\n";
        out << recent << ".000 Info\trecent entry\n";
        out << "recent continuation without timestamp\n";
    }
    const std::wstring previousLogPath = Logging::instance().getLogFilePathName();
    Logging::instance().setLogFilePathName(logPath.wstring());
    Logging::instance().retentionDays(7);
    LogCut()();

    std::string result;
    {
        // Eigener Block: Windows verweigert das Loeschen einer noch geoeffneten
        // Datei (siehe fs::remove() am Ende), POSIX unlinkt sie klaglos.
        std::ifstream in(logPath);
        std::stringstream content;
        content << in.rdbuf();
        result = content.str();
    }
    EXPECT_EQ(result.find("ancient"), std::string::npos);
    EXPECT_NE(result.find("recent entry"), std::string::npos);
    EXPECT_NE(result.find("recent continuation"), std::string::npos);

    if (!previousLogPath.empty())
        Logging::instance().setLogFilePathName(previousLogPath);
    fs::remove(logPath);
}
