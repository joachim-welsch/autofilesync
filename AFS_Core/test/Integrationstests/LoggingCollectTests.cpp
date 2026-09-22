/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of Autofilesync.
 *
 * AFS_Core is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AFS_Core is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AFS_Core. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include "ScopedLog.h"
#include <Logging.h>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace AFS_Core;

namespace {

std::string file_content(const std::filesystem::path& path)
{
    std::ostringstream content;
    content << std::ifstream(path).rdbuf();
    return content.str();
}

class CollectingStopped {  // [LoggingCollectTests.md#CollectingStopped]
public:
    ~CollectingStopped()
    {
        Logging::instance().collect_messages(false);
        Logging::instance().take_collected_messages();
    }
};

}

TEST(LoggingCollectTests, CollectsInfoAndAboveWithoutHeadWhileCollecting)
{
    const tu::ScopedLog log(std::filesystem::temp_directory_path() / "afs_collect_test.log", LogLevel::Info);
    const CollectingStopped stopped;
    Logging::instance().collect_messages(true);

    log_debug(L"nicht gesammelt");
    log_info(L"eine Information");
    log_warning(L"eine Warnung");
    log_error(L"ein Fehler");
    Logging::instance().collect_messages(false);
    log_error(L"nach dem Ende");

    const std::vector<LogMessage> messages = Logging::instance().take_collected_messages();
    ASSERT_EQ(messages.size(), 3u);
    EXPECT_EQ(messages[0].level, LogLevel::Info);
    EXPECT_EQ(messages[0].text, "eine Information");
    EXPECT_EQ(messages[1].level, LogLevel::Warning);
    EXPECT_EQ(messages[1].text, "eine Warnung");
    EXPECT_EQ(messages[2].level, LogLevel::Error);
    EXPECT_EQ(messages[2].text, "ein Fehler");
    EXPECT_TRUE(Logging::instance().take_collected_messages().empty()) << "take() hat die Meldungen nicht entnommen.";
}

TEST(LoggingCollectTests, CollectsBelowTheLogLevelWithoutWritingThemToTheFile)  // [LoggingCollectTests.md#below-the-level]
{
    const tu::ScopedLog log(std::filesystem::temp_directory_path() / "afs_collect_level_test.log", LogLevel::Error);
    const CollectingStopped stopped;
    Logging::instance().collect_messages(true);

    log_info(L"nur gesammelt");
    log_error(L"gesammelt und geschrieben");

    const std::vector<LogMessage> messages = Logging::instance().take_collected_messages();
    ASSERT_EQ(messages.size(), 2u);
    EXPECT_EQ(messages[0].text, "nur gesammelt");
    const std::string written = file_content(log.path());
    EXPECT_EQ(written.find("nur gesammelt"), std::string::npos) << "Die Info stand trotz Log-Level Error in der Datei:\n" << written;
    EXPECT_NE(written.find("gesammelt und geschrieben"), std::string::npos);
}

TEST(LoggingCollectTests, StartingAgainDropsTheEarlierMessages)
{
    const tu::ScopedLog log(std::filesystem::temp_directory_path() / "afs_collect_restart_test.log", LogLevel::Info);
    const CollectingStopped stopped;
    Logging::instance().collect_messages(true);
    log_error(L"alter Lauf");

    Logging::instance().collect_messages(true);
    log_error(L"neuer Lauf");

    const std::vector<LogMessage> messages = Logging::instance().take_collected_messages();
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].text, "neuer Lauf");
}

TEST(LoggingCollectTests, StopsAtTheCap)  // [LoggingCollectTests.md#cap]
{
    const tu::ScopedLog log(std::filesystem::temp_directory_path() / "afs_collect_cap_test.log", LogLevel::Error);
    const CollectingStopped stopped;
    Logging::instance().collect_messages(true);

    for (int i = 0; i < 250; ++i)
        log_info(L"Zeile " + std::to_wstring(i));

    EXPECT_EQ(Logging::instance().take_collected_messages().size(), 200u);
}
