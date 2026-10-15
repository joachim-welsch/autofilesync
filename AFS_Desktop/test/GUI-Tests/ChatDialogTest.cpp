/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
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
#include "GuiTest.h"
#include "EventPump.h"
#include "UserActions.h"
#include "ChatDialog.h"
#include "HelpBook.h"
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include "Config.h"
#include "GuiTestSandbox.h"
#include "SandboxTree.h"
#include "SyncActions.h"
#include "SyncItemDialog.h"
#include "TextResources.h"
#include "ModelLocation.h"
#include "Translation.h"
#include "id.h"
#include "utilwx.h"

using utilwx::tul;

namespace guitest {
namespace {

constexpr int REPLY_TIMEOUT_MILLISECONDS = 900000;  // [ChatDialogTest.md#REPLY_TIMEOUT_MILLISECONDS]

struct ChatControls {
    wxDialog* dialog = nullptr;
    wxWindow* input = nullptr;
    wxWindow* send = nullptr;
    wxWindow* transcript = nullptr;
};

ChatControls open_chat(MainWindow& window)  // [ChatDialogTest.md#open_chat]
{
    activate_menu_item(window, tul(id::Chat));
    ChatControls chat;
    wait_until([&chat] {
        chat.input = wxWindow::FindWindowById(ID_CHAT_INPUT);
        return chat.input != nullptr && chat.input->IsShownOnScreen();
    });
    if (chat.input == nullptr)
        return chat;
    chat.dialog = dynamic_cast<wxDialog*>(chat.input->GetParent());
    chat.send = wxWindow::FindWindowById(ID_CHAT_SEND);
    chat.transcript = wxWindow::FindWindowById(ID_CHAT_TRANSCRIPT);
    return chat;
}

class HiddenChat {  // [ChatDialogTest.md#HiddenChat]
public:
    explicit HiddenChat(ChatControls& chat): chat_(chat) {}
    ~HiddenChat()
    {
        if (chat_.send != nullptr)
            wait_until([this] { return chat_.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS);
        if (chat_.dialog != nullptr)
            chat_.dialog->Hide();
    }
private:
    ChatControls& chat_;
};

std::size_t count_of(const wxString& text, const wxString& part)
{
    std::size_t count = 0;
    for (std::size_t pos = text.find(part); pos != wxString::npos; pos = text.find(part, pos + part.length()))
        ++count;
    return count;
}

bool press_control_enter(wxWindow* input)
{
    if (!click_on(input))
        return false;
    const bool sent = press_return(wxMOD_CONTROL);
    pump();
    return sent;
}

}

TEST_F(GuiTest, AssistentHatEinEigenesMenue)  // [ChatDialogTest.md#AssistentHatEinEigenesMenue]
{
    wxMenuBar* menu_bar = main_window().GetMenuBar();
    const int assistant = menu_bar->FindMenu(AFS_Core::translation::translate("menu_assistant"));
    ASSERT_NE(assistant, wxNOT_FOUND) << "Die Menüleiste hat kein Menü Assistent.";
    EXPECT_NE(menu_bar->GetMenu(static_cast<size_t>(assistant))->FindItem(tul(id::Chat)), nullptr) << "Der Chat steht nicht im Menü Assistent.";
}

TEST_F(GuiTest, HilfebuchLiefertDemAssistentenSeineAbschnitte)  // [ChatDialogTest.md#HilfebuchLiefertDemAssistentenSeineAbschnitte]
{
    wxFileName book(wxStandardPaths::Get().GetExecutablePath());
    book.AppendDir("help");
    book.SetFullName("help.htb");
    ASSERT_TRUE(book.FileExists()) << "Kein Hilfebuch neben dem Testprogramm: " << book.GetFullPath();

    const std::vector<AFS_Core::help_knowledge::Section> sections = read_help_book(book.GetFullPath());

    EXPECT_GE(sections.size(), 20u) << "Das Hilfebuch lieferte zu wenige Abschnitte.";
    EXPECT_TRUE(std::ranges::any_of(sections, [](const auto& section) { return section.page == L"Hintergrunddienst"; }))
        << "Die Seite Hintergrunddienst fehlt oder ihr Titel kam falsch an.";
    EXPECT_TRUE(read_help_book(book.GetPath() + "/fehlt.htb").empty()) << "Ein fehlendes Hilfebuch lieferte Abschnitte.";
}

TEST_F(GuiTest, ChatLeereEingabeTutNichts)
{
    ChatControls chat = open_chat(main_window());
    ASSERT_NE(chat.input, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    const wxString transcript_before = value_of(chat.transcript);

    ASSERT_TRUE(click_when_enabled(chat.send));
    ASSERT_TRUE(press_control_enter(chat.input));

    EXPECT_EQ(value_of(chat.transcript), transcript_before) << "Eine leere Eingabe kam in den Verlauf.";
    EXPECT_TRUE(chat.send->IsEnabled()) << "Eine leere Eingabe hat den Dialog ins Warten versetzt.";
}

TEST_F(GuiTest, ChatNeuesGespraechLeertDenVerlauf)  // [ChatDialogTest.md#ChatNeuesGespraechLeertDenVerlauf]
{
    ChatControls chat = open_chat(main_window());
    ASSERT_NE(chat.input, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    if (LlamaClient::available())
        GTEST_SKIP() << "Mit Sprachmodell dauert die Antwort zu lange für diesen Test.";
    const wxString you = AFS_Core::translation::translate("chat_you") + ":";
    ASSERT_TRUE(type_into(chat.input, "Hallo"));
    ASSERT_TRUE(click_when_enabled(chat.send));
    ASSERT_TRUE(wait_until([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS)) << "Die Antwort kam nicht.";
    ASSERT_TRUE(value_of(chat.transcript).Contains(you)) << "Die Frage steht nicht im Verlauf.";

    ASSERT_TRUE(click_when_enabled(wxWindow::FindWindowById(ID_CHAT_NEW))) << "Der Knopf Neues Gespräch fehlt.";

    EXPECT_FALSE(value_of(chat.transcript).Contains(you)) << "Der Verlauf blieb stehen.";
    EXPECT_TRUE(value_of(chat.transcript).Contains(LlamaClient::missing_model_hint())) << "Der Hinweis vom Start fehlt.";
}

TEST_F(GuiTest, ChatSchliessenVerstecktUndMenueZeigtWieder)  // [ChatDialogTest.md#hidden-not-destroyed]
{
    ChatControls chat = open_chat(main_window());
    ASSERT_NE(chat.dialog, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    ASSERT_TRUE(type_into(chat.input, "Entwurf"));

    chat.dialog->Close();
    pump();
    EXPECT_FALSE(chat.dialog->IsShown()) << "Schließen hat den Chat nicht versteckt.";

    activate_menu_item(main_window(), tul(id::Chat));
    ASSERT_TRUE(wait_until([&chat] { return chat.dialog->IsShownOnScreen(); })) << "Der Chat kam über das Menü nicht zurück.";
    EXPECT_EQ(value_of(chat.input), "Entwurf") << "Der unfertige Text ging beim Schließen verloren.";
}

TEST_F(SlowGuiTest, ChatStrgEingabeSendet)  // [ChatDialogTest.md#ChatStrgEingabeSendet]
{
    ChatControls chat = open_chat(main_window());
    ASSERT_NE(chat.input, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    ASSERT_TRUE(wait_until([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS));
    ASSERT_TRUE(replace_text(chat.input, "Antworte nur mit dem Wort ja"));

    ASSERT_TRUE(press_control_enter(chat.input));

    EXPECT_NE(value_of(chat.transcript).Find("Antworte nur mit dem Wort ja"), wxNOT_FOUND) << "Strg+Eingabe hat die Frage nicht in den Verlauf gestellt.";
    EXPECT_EQ(value_of(chat.input), "") << "Das Eingabefeld wurde nach dem Senden nicht geleert.";
    EXPECT_TRUE(!chat.send->IsEnabled() || value_of(chat.transcript).Contains(AFS_Core::translation::translate("error") + L":"))  // [ChatDialogTest.md#no-model]
        << "Der Dialog wartet nach dem Senden nicht auf die Antwort.";
    ASSERT_TRUE(wait_until([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS)) << "Der Assistent hat nicht geantwortet.";
    EXPECT_TRUE(value_of(chat.transcript).Contains(AFS_Core::translation::translate("chat_assistant") + L":") || value_of(chat.transcript).Contains(AFS_Core::translation::translate("error") + L":"));
}

TEST_F(SlowGuiTest, ChatAntwortErscheintWaehrendSieEntsteht)  // [ChatDialogTest.md#ChatAntwortErscheintWaehrendSieEntsteht]
{
    if (!LlamaClient::available())
        GTEST_SKIP() << "Kein lokales Sprachmodell; " << model_location::PATH_VARIABLE << " ist nicht gesetzt.";
    ChatControls chat = open_chat(main_window());
    ASSERT_NE(chat.input, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    ASSERT_TRUE(wait_until([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS));
    ASSERT_TRUE(replace_text(chat.input, "Zaehle von 1 bis 40, jede Zahl in einer eigenen Zeile"));
    const wxString speaker = AFS_Core::translation::translate("chat_assistant") + L":";
    const std::size_t answers_before = count_of(value_of(chat.transcript), speaker);

    ASSERT_TRUE(press_control_enter(chat.input));

    ASSERT_TRUE(wait_until([&] { return count_of(value_of(chat.transcript), speaker) > answers_before || chat.send->IsEnabled(); },
        REPLY_TIMEOUT_MILLISECONDS)) << "Der Assistent hat nicht geantwortet.";
    EXPECT_FALSE(chat.send->IsEnabled()) << "Die Antwort erschien erst, als sie fertig war.";
    const wxString early = value_of(chat.transcript);
    ASSERT_TRUE(wait_until([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS));
    EXPECT_GT(value_of(chat.transcript).length(), early.length()) << "Die Antwort wuchs nach dem ersten Stück nicht weiter.";
    EXPECT_EQ(count_of(value_of(chat.transcript), speaker), answers_before + 1) << "Die fertige Antwort wurde ein zweites Mal angehängt.";
}

TEST_F(SlowGuiTest, ChatBeantwortetHilfefrageMitDerHilfeseite)  // [ChatDialogTest.md#ChatBeantwortetHilfefrageMitDerHilfeseite]
{
    if (!LlamaClient::available())
        GTEST_SKIP() << "Kein lokales Sprachmodell; " << model_location::PATH_VARIABLE << " ist nicht gesetzt.";
    ChatControls chat = open_chat(main_window());
    ASSERT_NE(chat.input, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    ASSERT_TRUE(wait_until([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS));
    ASSERT_TRUE(replace_text(chat.input, "Wie stelle ich die Sprache um?"));
    const wxString speaker = AFS_Core::translation::translate("chat_assistant") + L":";

    ASSERT_TRUE(press_control_enter(chat.input));

    ASSERT_TRUE(wait_until([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS)) << "Der Assistent hat nicht geantwortet.";
    const wxString transcript = value_of(chat.transcript);
    const wxString answer = transcript.Mid(transcript.rfind(speaker));  // [ChatDialogTest.md#last-answer-only]
    EXPECT_NE(answer.Find("Globale Einstellungen"), wxNOT_FOUND) << "Die Antwort nennt die Hilfeseite nicht: " << answer;
}

TEST_F(SlowGuiTest, ChatSchlaegtEinmalSynchronisierenVorUndFuehrtEsAus)  // [ChatDialogTest.md#ChatSchlaegtEinmalSynchronisierenVorUndFuehrtEsAus]
{
    if (!LlamaClient::available())
        GTEST_SKIP() << "Kein lokales Sprachmodell; " << model_location::PATH_VARIABLE << " ist nicht gesetzt.";
    const SandboxTree tree(GuiTestSandbox::root() / "chatbefehl", {{"quelle/bericht.txt", "Bericht"}, {"ziel/", ""}});
    SyncItemInput input{"Chat-Befehl", tree.path("quelle").string(), tree.path("ziel").string()};
    input.type = SyncItemType::Dir;
    ASSERT_TRUE(add_sync_item(main_window(), input));
    ChatControls chat = open_chat(main_window());
    ASSERT_NE(chat.input, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    ASSERT_TRUE(wait_until([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS));
    ASSERT_TRUE(replace_text(chat.input, "Bitte synchronisiere jetzt einmal."));
    wxWindow* button = wxWindow::FindWindowById(ID_CHAT_PROPOSAL);
    ASSERT_NE(button, nullptr);

    ASSERT_TRUE(press_control_enter(chat.input));
    ASSERT_TRUE(wait_until([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS)) << "Der Assistent hat nicht geantwortet.";
    ASSERT_TRUE(button->IsShown()) << "Die Antwort enthielt keinen Vorschlag: " << value_of(chat.transcript);
    EXPECT_EQ(button->GetLabel(), wxString(sync_proposal::label(sync_proposal::Command::SyncOnce)));
    ASSERT_TRUE(click_on(button));
    pump();

    ASSERT_TRUE(wait_until_sync_finished(main_window())) << "Die Synchronisation ist nicht zu Ende gelaufen.";
    EXPECT_EQ(tree.read("ziel/bericht.txt"), "Bericht") << "Der Knopf hat nicht synchronisiert.";
}

TEST_F(SlowGuiTest, ChatKenntDieAnzahlDerSynchronisationen)  // [ChatDialogTest.md#ChatKenntDieAnzahlDerSynchronisationen]
{
    if (!LlamaClient::available())
        GTEST_SKIP() << "Kein lokales Sprachmodell; " << model_location::PATH_VARIABLE << " ist nicht gesetzt.";
    const wxString source = GuiTestSandbox::source_dir().string(), target = GuiTestSandbox::target_dir().string();
    ASSERT_TRUE(add_sync_item(main_window(), SyncItemInput{"Chat-Eins", source, target}));
    ASSERT_TRUE(add_sync_item(main_window(), SyncItemInput{"Chat-Zwei", source, target}));
    ASSERT_TRUE(add_sync_item(main_window(), SyncItemInput{"Chat-Drei", source, target}));
    ChatControls chat = open_chat(main_window());
    ASSERT_NE(chat.input, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    ASSERT_TRUE(wait_until([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS));
    ASSERT_TRUE(replace_text(chat.input, "Wie viele Synchronisationen sind konfiguriert? Antworte nur mit der Zahl."));
    const wxString speaker = AFS_Core::translation::translate("chat_assistant") + L":";

    ASSERT_TRUE(press_control_enter(chat.input));

    ASSERT_TRUE(wait_until([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS)) << "Der Assistent hat nicht geantwortet.";
    const wxString transcript = value_of(chat.transcript);
    const wxString answer = transcript.Mid(transcript.rfind(speaker));  // [ChatDialogTest.md#last-answer-only]
    EXPECT_NE(answer.Find("3"), wxNOT_FOUND) << "Der Assistent kennt die Anzahl nicht: " << answer;
}

TEST_F(SlowGuiTest, ChatVorschlagLegtSynchronisationUeberDenDialogAn)  // [ChatDialogTest.md#ChatVorschlagLegtSynchronisationUeberDenDialogAn]
{
    if (!LlamaClient::available())
        GTEST_SKIP() << "Kein lokales Sprachmodell; " << model_location::PATH_VARIABLE << " ist nicht gesetzt.";
    const wxString source = GuiTestSandbox::source_dir().string(), target = GuiTestSandbox::target_dir().string();
    ChatControls chat = open_chat(main_window());
    ASSERT_NE(chat.input, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    ASSERT_TRUE(wait_until([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS));
    ASSERT_TRUE(replace_text(chat.input, "Lege eine neue Synchronisation mit dem Namen Chat-Vorschlag an. Quelle: "
        + source + " Ziel: " + target + " Typ: Verzeichnisbaum."));
    wxWindow* create = wxWindow::FindWindowById(ID_CHAT_PROPOSAL);
    ASSERT_NE(create, nullptr);
    EXPECT_FALSE(create->IsShown()) << "Der Anlegen-Knopf war schon vor dem Vorschlag sichtbar.";

    ASSERT_TRUE(press_control_enter(chat.input));
    ASSERT_TRUE(wait_until([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS)) << "Der Assistent hat nicht geantwortet.";
    const wxString transcript = value_of(chat.transcript);
    ASSERT_TRUE(create->IsShown()) << "Die Antwort enthielt keinen Vorschlag: " << transcript;
    EXPECT_EQ(transcript.Find("```"), wxNOT_FOUND) << "Der Vorschlagsblock steht im Verlauf.";  // [ChatDialogTest.md#block-not-shown]
    const wxString answer = transcript.Mid(transcript.rfind(AFS_Core::translation::translate("chat_assistant") + L":"));
    EXPECT_TRUE(answer.Contains("Chat-Vorschlag") && answer.Contains(target)) << "Die Antwort beschreibt den Vorschlag nicht: " << answer;  // [ChatDialogTest.md#proposal-described]

    wxString prefilled_source;
    ModalDialogWatcher::instance().expect(txt::add_synchronisation(), [&prefilled_source](wxDialog& dialog) {
        const bool confirmed = wait_until([&dialog] { return dialog.IsShownOnScreen(); })
            && (prefilled_source = value_of(find(dialog, ID_SOURCE_DIR_BOX)), true)
            && click_when_enabled(find(dialog, wxID_OK))
            && wait_until([&dialog] { return !dialog.IsShown(); });
        if (!confirmed)
            dialog.EndModal(wxID_CANCEL);
    });
    ASSERT_TRUE(click_on(create));
    pump();

    EXPECT_EQ(prefilled_source, source) << "Der Dialog war nicht mit der vorgeschlagenen Quelle vorbelegt.";
    const auto item = Config::instance().find_by_name(L"Chat-Vorschlag");
    ASSERT_TRUE(item.has_value()) << "Die Synchronisation kam nicht in die Konfiguration: " << transcript;
    EXPECT_EQ(item->source(), source.ToStdWstring());
    EXPECT_EQ(item->target(), target.ToStdWstring());
    EXPECT_FALSE(create->IsShown()) << "Der Anlegen-Knopf blieb nach dem Anlegen sichtbar.";

    ASSERT_TRUE(replace_text(chat.input, "Loesche die Synchronisation Chat-Vorschlag."));  // [ChatDialogTest.md#deletion-proposal]
    ASSERT_TRUE(press_control_enter(chat.input));
    ASSERT_TRUE(wait_until([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS)) << "Der Assistent hat nicht geantwortet.";
    ASSERT_TRUE(create->IsShown()) << "Die Antwort enthielt keinen Loeschvorschlag: " << value_of(chat.transcript);
    EXPECT_EQ(create->GetLabel(), wxString(AFS_Core::translation::translate("chat_delete_sync")));
    ModalDialogWatcher::instance().answer(AFS_Core::translation::translate("confirm_delete_title"), wxID_YES);
    ASSERT_TRUE(click_on(create));
    pump();
    EXPECT_FALSE(Config::instance().find_by_name(L"Chat-Vorschlag").has_value()) << "Die Synchronisation wurde nicht geloescht.";
    EXPECT_FALSE(create->IsShown());
}

}
