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
#include "HelpKnowledge.h"
#include "Util.h"
#include <filesystem>

using namespace AFS_Core;
namespace fs = std::filesystem;
using help_knowledge::Section;

namespace {

const std::wstring PAGE = LR"(<html><head><title>Titel</title></head><body>
<h1>Globale Einstellungen</h1>
<p><b>Bearbeiten &rarr; Globale Einstellungen</b> öffnet
die Einstellungen.</p>
<h2>Sprache</h2>
<p>Die Auswahl <b>Sprache</b> legt die Sprache fest.</p>
<ul><li>Deutsch</li><li>Englisch</li></ul>
<h2>Starten und Fenster</h2>
<table><tr><td><b>Start minimiert</b></td><td>Das Fenster erscheint nicht.</td></tr></table>
</body></html>)";

std::vector<Section> real_help()  // [HelpKnowledgeTests.md#real_help]
{
    std::vector<Section> sections;
    const fs::path help = fs::path(LIB_SOURCE_DIR) / ".." / "AFS_Desktop" / "help";
    for (const auto& entry : fs::directory_iterator(help))
        if (entry.path().extension() == ".html") {
            const std::vector<Section> page = help_knowledge::sections_of(util::read_utf8_file(entry.path()));
            sections.insert(sections.end(), page.begin(), page.end());
        }
    return sections;
}

}

TEST(HelpKnowledgeTests, PageIsSplitAtItsSubheadings)
{
    const std::vector<Section> sections = help_knowledge::sections_of(PAGE);

    ASSERT_EQ(sections.size(), 3u);
    EXPECT_EQ(sections[0].page, L"Globale Einstellungen");
    EXPECT_EQ(sections[0].heading, L"");
    EXPECT_EQ(sections[0].text, L"Bearbeiten → Globale Einstellungen öffnet die Einstellungen.");
    EXPECT_EQ(sections[1].heading, L"Sprache");
    EXPECT_EQ(sections[1].text, L"Die Auswahl Sprache legt die Sprache fest.\n- Deutsch\n- Englisch");
    EXPECT_EQ(sections[2].text, L"Start minimiert Das Fenster erscheint nicht.");
}

TEST(HelpKnowledgeTests, SectionWithTheMatchingHeadingIsChosen)
{
    const std::wstring excerpt = help_knowledge::excerpt_for(L"Wie stelle ich die Sprache um?", help_knowledge::sections_of(PAGE), 2500);

    EXPECT_EQ(excerpt.rfind(L"[Globale Einstellungen – Sprache]\n", 0), 0u) << util::wstring_to_string(excerpt);
}

TEST(HelpKnowledgeTests, SmallTalkGetsNoExcerpt)  // [HelpKnowledgeTests.md#SmallTalkGetsNoExcerpt]
{
    EXPECT_TRUE(help_knowledge::excerpt_for(L"Wie geht es dir?", real_help(), 2500).empty());
}

TEST(HelpKnowledgeTests, ExcerptStaysWithinItsBudget)
{
    const std::wstring excerpt = help_knowledge::excerpt_for(L"Wie richte ich eine SSH-Verbindung ein?", real_help(), 300);

    EXPECT_FALSE(excerpt.empty());
    EXPECT_LE(excerpt.size(), 300u);
}

TEST(HelpKnowledgeTests, TypicalQuestionsFindTheirHelpPage)  // [HelpKnowledgeTests.md#TypicalQuestionsFindTheirHelpPage]
{
    const std::vector<Section> help = real_help();
    const std::pair<const wchar_t*, const wchar_t*> cases[] = {
        {L"Wie stelle ich die Sprache um?", L"[Globale Einstellungen – Sprache]"},
        {L"Was macht der Hintergrunddienst?", L"[Hintergrunddienst"},
        {L"Wo liegt die Konfigurationsdatei?", L"[Konfigurationsdatei – Wo sie liegt]"},
        {L"Wie kann ich gelöschte Dateien zurückholen?", L"[Wenn etwas nicht wie erwartet passiert – Gelöschte Dateien zurückholen]"},
        {L"Was ist ein Sicherungsnachweis?", L"[Sicherungsnachweis"}};
    for (const auto& [question, label] : cases) {
        const std::wstring excerpt = help_knowledge::excerpt_for(question, help, 2500);
        EXPECT_EQ(excerpt.rfind(label, 0), 0u) << util::wstring_to_string(question) << "\n" << util::wstring_to_string(excerpt.substr(0, 120));
    }
}
