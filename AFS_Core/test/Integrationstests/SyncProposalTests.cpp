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
#include "SyncProposal.h"
#include <clocale>
#include <string>
#include "ScopedEnv.h"
#include "StrUtil.h"
#include "Translation.h"

using namespace AFS_Core;
using std::wstring;

namespace {
const wstring BLOCK = L"```sync\n{\"name\": \"Fotos\", \"source\": \"/home/jw/Fotos\", \"target\": \"/media/nas/Fotos\", \"type\": \"Verzeichnisbaum\"}\n```";
const wstring PROSE = L"Gern, hier ist mein Vorschlag:";
}

TEST(SyncProposalTests, ParsesFencedBlockAndStripsIt)
{
    const wstring answer = PROSE + L"\n\n" + BLOCK + L"\nPrüfe bitte die Pfade.";
    const auto proposal = sync_proposal::parse(answer);
    ASSERT_TRUE(proposal.has_value());
    const SyncItem& item = std::get<SyncItem>(*proposal);
    EXPECT_EQ(item.name(), L"Fotos");
    EXPECT_EQ(item.source(), L"/home/jw/Fotos");
    EXPECT_EQ(item.target(), L"/media/nas/Fotos");
    EXPECT_EQ(item.sync_item_type(), SyncItemType::Tree);
    EXPECT_FALSE(item.two_way_sync());
    EXPECT_EQ(sync_proposal::without_block(answer), PROSE + L"\n\n\nPrüfe bitte die Pfade.");
}

TEST(SyncProposalTests, AnswerWithoutBlockIsNoProposal)
{
    const wstring answer = L"Es sind 3 Synchronisationen konfiguriert.";
    EXPECT_FALSE(sync_proposal::parse(answer).has_value());
    EXPECT_EQ(sync_proposal::without_block(answer), answer);
}

TEST(SyncProposalTests, MissingPathIsNoProposal)  // [SyncProposalTests.md#MissingPathIsNoProposal]
{
    EXPECT_FALSE(sync_proposal::parse(L"```sync\n{\"name\": \"Fotos\", \"source\": \"/home/jw/Fotos\", \"target\": \"\"}\n```").has_value());
    EXPECT_FALSE(sync_proposal::parse(L"```sync\n{\"name\": \"Fotos\", \"source\": \"/home/jw/Fotos\"}\n```").has_value());
}

TEST(SyncProposalTests, BrokenJsonIsNoProposal)
{
    const wstring answer = L"```sync\n{\"name\": \"Fotos\", \"source\": \"/a\", \"target\": \"/b\"\n```";
    EXPECT_FALSE(sync_proposal::parse(answer).has_value());
    EXPECT_EQ(sync_proposal::without_block(answer), answer);
}

TEST(SyncProposalTests, BareJsonWithoutFenceIsAccepted)  // [SyncProposalTests.md#BareJsonWithoutFenceIsAccepted]
{
    const wstring answer = L"Vorschlag: {\"name\": \"Doku\", \"source\": \"/a\", \"target\": \"/b\", \"type\": \"Verzeichnis\", \"two_way\": true}";
    const auto proposal = sync_proposal::parse(answer);
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(std::get<SyncItem>(*proposal).sync_item_type(), SyncItemType::Dir);
    EXPECT_TRUE(std::get<SyncItem>(*proposal).two_way_sync());
    EXPECT_EQ(sync_proposal::without_block(answer), L"Vorschlag:");
}

TEST(SyncProposalTests, UnknownTypeBecomesTree)
{
    const auto proposal = sync_proposal::parse(L"```json\n{\"name\": \"x\", \"source\": \"/a\", \"target\": \"/b\", \"type\": \"Ordner\"}\n```");
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(std::get<SyncItem>(*proposal).sync_item_type(), SyncItemType::Tree);
}

TEST(SyncProposalTests, FirstBlockWithoutProposalIsSkipped)
{
    const wstring answer = L"```\nls -l\n```\n" + BLOCK;
    const auto proposal = sync_proposal::parse(answer);
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(std::get<SyncItem>(*proposal).name(), L"Fotos");
    EXPECT_EQ(sync_proposal::without_block(answer), L"```\nls -l\n```");
}

TEST(SyncProposalTests, InstructionNamesTypesAndFields)  // [SyncProposalTests.md#InstructionNamesTypesAndFields]
{
    if (translation::locale_dir().empty())
        GTEST_SKIP() << "no compiled catalogues beside the test binary";
    const std::string saved_locale = std::setlocale(LC_ALL, nullptr);
    translation::init_locale(translation::DEFAULT_LOCALES);
    const ScopedEnv language("LANGUAGE", "de");
    translation::init(translation::locale_dir());
    const wstring instruction = sync_proposal::instruction();
    const wstring tree = sync_item_type_to_str(SyncItemType::Tree);  // read while the catalogue is still bound
    std::setlocale(LC_ALL, saved_locale.c_str());
    EXPECT_NE(instruction.find(tree), wstring::npos) << instruction;
    EXPECT_NE(instruction.find(L"\"source\""), wstring::npos) << instruction;
    EXPECT_NE(instruction.find(L"\"target\""), wstring::npos) << instruction;
}

TEST(SyncProposalTests, TwoPathsInAQuestionTriggerTheExtraction)  // [SyncProposalTests.md#TwoPathsInAQuestionTriggerTheExtraction]
{
    EXPECT_TRUE(sync_proposal::names_two_paths(L"Erstelle eine Synchronisation names Test1, Quelle /home/jw/a.txt Ziel: /tmp"));
    EXPECT_TRUE(sync_proposal::names_two_paths(L"Sichere C:\\Daten nach PiSpoccia:/home/pi/mirror"));
    EXPECT_FALSE(sync_proposal::names_two_paths(L"Lege eine Synchronisation von Fotos auf das NAS an."));
    EXPECT_FALSE(sync_proposal::names_two_paths(L"Was liegt in /tmp?"));
}

TEST(SyncProposalTests, GrammarAllowsOnlyObjectOrNull)
{
    const std::string grammar = sync_proposal::grammar({});
    EXPECT_EQ(grammar.find("deletion"), std::string::npos) << grammar;
    EXPECT_NE(grammar.find("root ::= object | \"null\""), std::string::npos) << grammar;
    EXPECT_NE(grammar.find("\\\"source\\\""), std::string::npos) << grammar;
    EXPECT_NE(grammar.find("\\\"target\\\""), std::string::npos) << grammar;
    EXPECT_NE(grammar.find(util::wstring_to_string(sync_item_type_to_str(SyncItemType::Tree))), std::string::npos) << grammar;
}

TEST(SyncProposalTests, ExtractedObjectBecomesAParsableBlock)
{
    const wstring block = sync_proposal::as_block(L"{\"name\": \"Test1\", \"source\": \"/a\", \"target\": \"/b\", \"type\": \"Datei\"}");
    const auto proposal = sync_proposal::parse(L"Das geht so:\n" + block);
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(std::get<SyncItem>(*proposal).name(), L"Test1");
    EXPECT_EQ(std::get<SyncItem>(*proposal).sync_item_type(), SyncItemType::File);
    EXPECT_EQ(sync_proposal::without_block(L"Das geht so:\n" + block), L"Das geht so:");
}

TEST(SyncProposalTests, DeleteBlockIsADeletion)  // [SyncProposalTests.md#DeleteBlockIsADeletion]
{
    const wstring answer = L"Gern.\n```sync\n{\"delete\": \"Fotos\"}\n```";
    const auto proposal = sync_proposal::parse(answer);
    ASSERT_TRUE(proposal.has_value());
    ASSERT_TRUE(std::holds_alternative<sync_proposal::Deletion>(*proposal));
    EXPECT_EQ(std::get<sync_proposal::Deletion>(*proposal).name, L"Fotos");
    EXPECT_EQ(sync_proposal::without_block(answer), L"Gern.");
    EXPECT_FALSE(sync_proposal::parse(L"```sync\n{\"delete\": \"\"}\n```").has_value());
    EXPECT_FALSE(sync_proposal::parse(L"```sync\n{\"name\": \"Fotos\"}\n```").has_value());
}

TEST(SyncProposalTests, ConfiguredNameInAQuestionTriggersTheExtraction)
{
    const SyncItems items = {SyncItem(L"Fotos", SyncItemType::Tree, L"/a", L"/b", L"*"), SyncItem(L"Mein Backup", SyncItemType::Dir, L"/c", L"/d", L"*")};
    EXPECT_TRUE(sync_proposal::names_configured_item(L"Lösche die Synchronisation fotos", items));
    EXPECT_TRUE(sync_proposal::names_configured_item(L"Entferne mein backup bitte", items));
    EXPECT_FALSE(sync_proposal::names_configured_item(L"Lösche die Synchronisation Videos", items));
    EXPECT_FALSE(sync_proposal::names_configured_item(L"Lösche Fotos", {}));
}

TEST(SyncProposalTests, GrammarRestrictsDeletionToConfiguredNames)  // [SyncProposalTests.md#GrammarRestrictsDeletionToConfiguredNames]
{
    const SyncItems items = {SyncItem(L"Fotos", SyncItemType::Tree, L"/a", L"/b", L"*"), SyncItem(L"Say \"hi\"", SyncItemType::Dir, L"/c", L"/d", L"*")};
    const std::string grammar = sync_proposal::grammar(items);
    EXPECT_NE(grammar.find("root ::= object | deletion | \"null\""), std::string::npos) << grammar;
    EXPECT_NE(grammar.find("configured ::= \"\\\"Fotos\\\"\" | \"\\\"Say \\\\\\\"hi\\\\\\\"\\\"\""), std::string::npos) << grammar;
}

TEST(SyncProposalTests, DeletionDescriptionNamesItemAndButton)
{
    if (translation::locale_dir().empty())
        GTEST_SKIP() << "no compiled catalogues beside the test binary";
    const std::string saved_locale = std::setlocale(LC_ALL, nullptr);
    translation::init_locale(translation::DEFAULT_LOCALES);
    const ScopedEnv language("LANGUAGE", "de");
    translation::init(translation::locale_dir());
    const wstring description = sync_proposal::describe(sync_proposal::Deletion{L"Fotos"});
    const wstring button = translation::translate("chat_delete_sync");
    std::setlocale(LC_ALL, saved_locale.c_str());
    EXPECT_NE(description.find(L"Fotos"), wstring::npos) << description;
    EXPECT_NE(description.find(button), wstring::npos) << description;
    EXPECT_EQ(description.find(L'%'), wstring::npos) << description;
}

TEST(SyncProposalTests, DescriptionNamesItemAndButton)  // [SyncProposalTests.md#DescriptionNamesItemAndButton]
{
    if (translation::locale_dir().empty())
        GTEST_SKIP() << "no compiled catalogues beside the test binary";
    const std::string saved_locale = std::setlocale(LC_ALL, nullptr);
    translation::init_locale(translation::DEFAULT_LOCALES);
    const ScopedEnv language("LANGUAGE", "de");
    translation::init(translation::locale_dir());
    const auto proposal = sync_proposal::parse(BLOCK);
    ASSERT_TRUE(proposal.has_value());
    const wstring description = sync_proposal::describe(*proposal);
    const wstring tree = sync_item_type_to_str(SyncItemType::Tree), button = translation::translate("chat_create_sync");
    std::setlocale(LC_ALL, saved_locale.c_str());
    for (const wstring& part : {wstring(L"Fotos"), wstring(L"/home/jw/Fotos"), wstring(L"/media/nas/Fotos"), tree, button})
        EXPECT_NE(description.find(part), wstring::npos) << description;
    EXPECT_EQ(description.find(L'%'), wstring::npos) << description;
}
