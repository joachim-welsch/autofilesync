#include "pch.h"
#include <LogAnalysis.h>
#include <Logging.h>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace AFS_Core;

namespace fs = std::filesystem;

namespace {
// Points Logging at a temporary file for the duration of the test and
// restores the previous path afterwards, mirroring LogCutTests.cpp.
class ScopedLogPath {
    fs::path path_;
    std::wstring previous_;
public:
    explicit ScopedLogPath(const std::string& content)
        : path_(fs::temp_directory_path() / ("autofilesync_loganalysis_" + std::to_string(getpid()) + ".log"))
        , previous_(Logging::instance().getLogFilePathName())
    {
        fs::remove(path_);
        { std::ofstream out(path_); out << content; }
        Logging::instance().setLogFilePathName(path_.wstring());
    }
    ~ScopedLogPath()
    {
        if (!previous_.empty())
            Logging::instance().setLogFilePathName(previous_);
        fs::remove(path_);
    }
};
}

TEST(LogAnalysisTests, ReturnsWholeFileWhenUnderBudget)
{
    ScopedLogPath scoped("2024-01-01 10:00:00.000 Info\tfirst entry\n2024-01-01 10:00:01.000 Error\tsecond entry\n");
    const std::wstring result = readRecentLogEntries(1000);
    EXPECT_NE(result.find(L"first entry"), std::wstring::npos);
    EXPECT_NE(result.find(L"second entry"), std::wstring::npos);
}

// The excerpt must start at a real log entry, not mid-line: a truncated first
// line would misrepresent the entry it belongs to.
TEST(LogAnalysisTests, TruncatesToWholeLinesWhenOverBudget)
{
    ScopedLogPath scoped("2024-01-01 10:00:00.000 Info\told entry that must be dropped\n"
                          "2024-01-01 10:00:01.000 Error\trecent entry that must survive\n");
    const std::wstring result = readRecentLogEntries(80);
    EXPECT_EQ(result.find(L"old entry"), std::wstring::npos);
    EXPECT_NE(result.find(L"recent entry that must survive"), std::wstring::npos);
    EXPECT_EQ(result.find(L"\told entry"), std::wstring::npos);
}

TEST(LogAnalysisTests, ReturnsEmptyForMissingFile)
{
    const std::wstring previous = Logging::instance().getLogFilePathName();
    const fs::path missing = fs::temp_directory_path() / ("autofilesync_loganalysis_missing_" + std::to_string(getpid()) + ".log");
    fs::remove(missing);
    Logging::instance().setLogFilePathName(missing.wstring());
    EXPECT_TRUE(readRecentLogEntries(1000).empty());
    if (!previous.empty())
        Logging::instance().setLogFilePathName(previous);
}

TEST(LogAnalysisTests, RecognisesLogAndProtokollKeywordsCaseInsensitively)
{
    EXPECT_TRUE(looksLikeLogQuestion(L"Was steht im Log?"));
    EXPECT_TRUE(looksLikeLogQuestion(L"WARUM GIBT ES FEHLER IM LOG"));
    EXPECT_TRUE(looksLikeLogQuestion(L"Kannst du die Protokolldatei pruefen?"));
}

TEST(LogAnalysisTests, DoesNotFlagUnrelatedQuestions)
{
    EXPECT_FALSE(looksLikeLogQuestion(L"Wie lege ich eine neue Synchronisation an?"));
    EXPECT_FALSE(looksLikeLogQuestion(L"Was ist Autofilesync?"));
}

// The chat question ChatDialog::onSend() actually routes through the log path
// for: detection must fire, and the excerpt it attaches to the request must
// contain the warning entries the question asks about.
TEST(LogAnalysisTests, WarningsQuestionIsRecognisedAndFindsWarningEntries)
{
    ScopedLogPath scoped("2024-01-01 10:00:00.000 Info\tsync started\n"
                          "2024-01-01 10:00:01.000 Warning\tZielverzeichnis fast voll\n"
                          "2024-01-01 10:00:02.000 Info\tsync finished\n");
    const std::wstring question = L"Gibt es Warnings im Log?";
    EXPECT_TRUE(looksLikeLogQuestion(question));
    const std::wstring excerpt = readRecentLogEntries(1000);
    EXPECT_NE(excerpt.find(L"Warning"), std::wstring::npos);
    EXPECT_NE(excerpt.find(L"Zielverzeichnis fast voll"), std::wstring::npos);
}
