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
#include <LogAnalysis.h>
#include <Logging.h>
#include <filesystem>
#include <fstream>
#include "ScopedLog.h"
#include <ScopedEnv.h>
#include <Translation.h>
#include <clocale>
#include <optional>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace AFS_Core;

namespace fs = std::filesystem;

namespace {
class ScopedLogPath {  // [LogAnalysisTests.md#ScopedLogPath]
    fs::path path_;
    std::wstring previous_;
public:
    explicit ScopedLogPath(const std::string& content)
        : path_(fs::temp_directory_path() / ("autofilesync_loganalysis_" + std::to_string(getpid()) + ".log"))
        , previous_(Logging::instance().get_log_file_path_name())
    {
        fs::remove(path_);
        { std::ofstream out(path_); out << content; }
        Logging::instance().set_log_file_path_name(path_.wstring());
    }
    ~ScopedLogPath()
    {
        if (!previous_.empty())
            Logging::instance().set_log_file_path_name(previous_);
        fs::remove(path_);
    }
};
}

TEST(LogAnalysisTests, ReturnsWholeFileWhenUnderBudget)
{
    ScopedLogPath scoped("2024-01-01 10:00:00.000 Info\tfirst entry\n2024-01-01 10:00:01.000 Error\tsecond entry\n");
    const std::wstring result = read_recent_log_entries(1000);
    EXPECT_NE(result.find(L"first entry"), std::wstring::npos);
    EXPECT_NE(result.find(L"second entry"), std::wstring::npos);
}

TEST(LogAnalysisTests, TruncatesToWholeLinesWhenOverBudget)  // [LogAnalysisTests.md#TruncatesToWholeLinesWhenOverBudget]
{
    ScopedLogPath scoped("2024-01-01 10:00:00.000 Info\told entry that must be dropped\n"
                          "2024-01-01 10:00:01.000 Error\trecent entry that must survive\n");
    const std::wstring result = read_recent_log_entries(80);
    EXPECT_EQ(result.find(L"old entry"), std::wstring::npos);
    EXPECT_NE(result.find(L"recent entry that must survive"), std::wstring::npos);
    EXPECT_EQ(result.find(L"\told entry"), std::wstring::npos);
}

TEST(LogAnalysisTests, ReturnsEmptyForMissingFile)
{
    const tu::ScopedLog log(fs::temp_directory_path() / ("autofilesync_loganalysis_missing_" + std::to_string(getpid()) + ".log"));
    EXPECT_TRUE(read_recent_log_entries(1000).empty());
}

class LogWordsTest : public ::testing::Test {  // [LogAnalysisTests.md#LogWordsTest]
protected:
    std::string saved_locale_ = std::setlocale(LC_ALL, nullptr);
    std::optional<ScopedEnv> language_;

    void SetUp() override
    {
        if (translation::locale_dir().empty())
            GTEST_SKIP() << "no compiled catalogues beside the test binary";
        language_.emplace("LANGUAGE", nullptr);
        translation::init_locale(translation::DEFAULT_LOCALES);
#ifdef LC_MESSAGES
        if (std::string(std::setlocale(LC_MESSAGES, nullptr)) == "C")
            GTEST_SKIP() << "no usable locale, gettext ignores LANGUAGE in C";
#endif
        translation::init(translation::locale_dir());
    }

    void TearDown() override
    {
        language_.reset();
        std::setlocale(LC_ALL, saved_locale_.c_str());  // with C restored, later tests see the bare keys as before
    }
};

TEST_F(LogWordsTest, RecognisesLogAndProtokollKeywordsCaseInsensitively)
{
    translation::set_language("de");
    EXPECT_TRUE(looks_like_log_question(L"Was steht im Log?"));
    EXPECT_TRUE(looks_like_log_question(L"WARUM GIBT ES FEHLER IM LOG"));
    EXPECT_TRUE(looks_like_log_question(L"Kannst du die Protokolldatei pruefen?"));
}

TEST_F(LogWordsTest, RecognisesTheLogWordOfEveryLanguage)  // [LogAnalysisTests.md#RecognisesTheLogWordOfEveryLanguage]
{
    const std::pair<const char*, const wchar_t*> questions[] = {
        {"es", L"¿Qué dice el registro?"}, {"fr", L"Que dit le journal ?"}, {"it", L"Cosa dice il registro?"},
        {"pl", L"Co jest w dzienniku?"}, {"uk", L"Що в журналі?"}};
    for (const auto& [code, question] : questions) {
        translation::set_language(code);
        EXPECT_TRUE(looks_like_log_question(question)) << code;
    }
}

TEST(LogAnalysisTests, DoesNotFlagUnrelatedQuestions)
{
    EXPECT_FALSE(looks_like_log_question(L"Wie lege ich eine neue Synchronisation an?"));
    EXPECT_FALSE(looks_like_log_question(L"Was ist Autofilesync?"));
}

TEST(LogAnalysisTests, WarningsQuestionIsRecognisedAndFindsWarningEntries)  // [LogAnalysisTests.md#WarningsQuestionIsRecognisedAndFindsWarningEntries]
{
    ScopedLogPath scoped("2024-01-01 10:00:00.000 Info\tsync started\n"
                          "2024-01-01 10:00:01.000 Warning\tZielverzeichnis fast voll\n"
                          "2024-01-01 10:00:02.000 Info\tsync finished\n");
    const std::wstring question = L"Gibt es Warnings im Log?";
    EXPECT_TRUE(looks_like_log_question(question));
    const std::wstring excerpt = read_recent_log_entries(1000);
    EXPECT_NE(excerpt.find(L"Warning"), std::wstring::npos);
    EXPECT_NE(excerpt.find(L"Zielverzeichnis fast voll"), std::wstring::npos);
}
