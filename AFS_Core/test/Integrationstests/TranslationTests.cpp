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
#include <ScopedEnv.h>
#include "ScopedLog.h"
#include <chrono>
#include <filesystem>
#include <Translation.h>
#include <clocale>

using namespace AFS_Core;

namespace {

class TranslationTest : public ::testing::Test {
protected:
    std::string saved_locale_ = std::setlocale(LC_ALL, nullptr);

    void SetUp() override
    {
        if (translation::locale_dir().empty())
            GTEST_SKIP() << "no compiled catalogues beside the test binary";
        translation::init_locale(translation::DEFAULT_LOCALES);
#ifdef LC_MESSAGES
        if (std::string(std::setlocale(LC_MESSAGES, nullptr)) == "C")
            GTEST_SKIP() << "no usable locale, gettext ignores LANGUAGE in C";
#endif
    }

    void TearDown() override
    {
        std::setlocale(LC_ALL, saved_locale_.c_str());  // with C restored, later tests see the bare keys as before
    }

    static std::string translated_in(const char* language, const char* msgid)
    {
        ScopedEnv env("LANGUAGE", language);  // also undoes the "en" that init() may set
        translation::init(translation::locale_dir());
        return translation::translate_utf8(msgid);
    }
};

TEST_F(TranslationTest, LanguageWithCatalogueKeepsIt)
{
    EXPECT_EQ(translated_in("de", "already_running"), "AutoFileSync läuft bereits.");
}

TEST_F(TranslationTest, ChosenLanguageAppliesAtOnce)
{
    ScopedEnv env("LANGUAGE", "de");
    translation::init(translation::locale_dir());
    translation::set_language("en");
    EXPECT_EQ(translation::translate_utf8("already_running"), "AutoFileSync is already running.");
}

TEST_F(TranslationTest, NoChosenLanguageReturnsToEnvironment)  // [TranslationTests.md#NoChosenLanguageReturnsToEnvironment]
{
    ScopedEnv env("LANGUAGE", "de");
    translation::init(translation::locale_dir());
    translation::set_language("fr");
    translation::set_language("");
    EXPECT_EQ(translation::translate_utf8("already_running"), "AutoFileSync läuft bereits.");
}

TEST_F(TranslationTest, ChosenLanguageWithoutCatalogueFallsBackToEnglish)
{
    ScopedEnv env("LANGUAGE", "de");
    translation::init(translation::locale_dir());
    translation::set_language("sv");
    EXPECT_EQ(translation::translate_utf8("already_running"), "AutoFileSync is already running.");
}

TEST_F(TranslationTest, LanguageWithoutCatalogueFallsBackToEnglish)  // [TranslationTests.md#LanguageWithoutCatalogueFallsBackToEnglish]
{
    EXPECT_EQ(translated_in("sv", "already_running"), "AutoFileSync is already running.");
}

}

TEST_F(TranslationTest, CatalogueOfThisBuildMatches)  // [TranslationTests.md#CatalogueOfThisBuildMatches]
{
    ScopedEnv env("LANGUAGE", "de");
    translation::init(translation::locale_dir());
    const auto mismatch = translation::catalogue_mismatch();
    EXPECT_FALSE(mismatch.has_value()) << (mismatch ? *mismatch : L"");
}

TEST_F(TranslationTest, MissingTranslationIsLoggedOnce)  // [TranslationTests.md#MissingTranslationIsLoggedOnce]
{
    ScopedEnv env("LANGUAGE", "de");
    translation::init(translation::locale_dir());
    const tu::ScopedLog log(std::filesystem::temp_directory_path() / "afs_translation_test.log", LogLevel::Info);
    const std::string msgid = "no_such_message_id_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    Logging::instance().collect_messages(true);
    EXPECT_EQ(translation::translate_utf8(msgid.c_str()), msgid);
    EXPECT_EQ(translation::translate_utf8(msgid.c_str()), msgid);
    Logging::instance().collect_messages(false);
    const std::vector<LogMessage> messages = Logging::instance().take_collected_messages();
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_NE(messages[0].text.find(msgid), std::string::npos) << messages[0].text;
}

TEST_F(TranslationTest, CatalogueLanguageNamesTheCatalogueInUse)  // [TranslationTests.md#CatalogueLanguageNamesTheCatalogueInUse]
{
    ScopedEnv env("LANGUAGE", "de");
    translation::init(translation::locale_dir());
    EXPECT_EQ(translation::catalogue_language(), "de");
    translation::set_language("uk");
    EXPECT_EQ(translation::catalogue_language(), "uk");
    translation::set_language("sv");
    EXPECT_EQ(translation::catalogue_language(), "en") << "Without a Swedish catalogue the English one is in use.";
}
