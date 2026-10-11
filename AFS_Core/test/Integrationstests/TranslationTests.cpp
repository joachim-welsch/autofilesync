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

TEST_F(TranslationTest, LanguageWithoutCatalogueFallsBackToEnglish)  // [TranslationTests.md#LanguageWithoutCatalogueFallsBackToEnglish]
{
    EXPECT_EQ(translated_in("sv", "already_running"), "AutoFileSync is already running.");
}

}
