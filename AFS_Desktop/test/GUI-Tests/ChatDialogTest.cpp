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
#include "Config.h"
#include "GuiTestSandbox.h"
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

constexpr int REPLY_TIMEOUT_MILLISECONDS = 600000;  // [ChatDialogTest.md#REPLY_TIMEOUT_MILLISECONDS]

struct ChatControls {
    wxDialog* dialog = nullptr;
    wxWindow* input = nullptr;
    wxWindow* send = nullptr;
    wxWindow* transcript = nullptr;
};

ChatControls openChat(MainWindow& window)  // [ChatDialogTest.md#openChat]
{
    activateMenuItem(window, tul(id::Chat));
    ChatControls chat;
    waitUntil([&chat] {
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
            waitUntil([this] { return chat_.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS);
        if (chat_.dialog != nullptr)
            chat_.dialog->Hide();
    }
private:
    ChatControls& chat_;
};

std::size_t countOf(const wxString& text, const wxString& part)
{
    std::size_t count = 0;
    for (std::size_t pos = text.find(part); pos != wxString::npos; pos = text.find(part, pos + part.length()))
        ++count;
    return count;
}

bool pressControlEnter(wxWindow* input)
{
    if (!clickOn(input))
        return false;
    const bool sent = pressReturn(wxMOD_CONTROL);
    pump();
    return sent;
}

}

TEST_F(GuiTest, ChatLeereEingabeTutNichts)
{
    ChatControls chat = openChat(mainWindow());
    ASSERT_NE(chat.input, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    const wxString transcriptBefore = valueOf(chat.transcript);

    ASSERT_TRUE(clickWhenEnabled(chat.send));
    ASSERT_TRUE(pressControlEnter(chat.input));

    EXPECT_EQ(valueOf(chat.transcript), transcriptBefore) << "Eine leere Eingabe kam in den Verlauf.";
    EXPECT_TRUE(chat.send->IsEnabled()) << "Eine leere Eingabe hat den Dialog ins Warten versetzt.";
}

TEST_F(GuiTest, ChatSchliessenVerstecktUndMenueZeigtWieder)  // [ChatDialogTest.md#hidden-not-destroyed]
{
    ChatControls chat = openChat(mainWindow());
    ASSERT_NE(chat.dialog, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    ASSERT_TRUE(typeInto(chat.input, "Entwurf"));

    chat.dialog->Close();
    pump();
    EXPECT_FALSE(chat.dialog->IsShown()) << "Schließen hat den Chat nicht versteckt.";

    activateMenuItem(mainWindow(), tul(id::Chat));
    ASSERT_TRUE(waitUntil([&chat] { return chat.dialog->IsShownOnScreen(); })) << "Der Chat kam über das Menü nicht zurück.";
    EXPECT_EQ(valueOf(chat.input), "Entwurf") << "Der unfertige Text ging beim Schließen verloren.";
}

TEST_F(SlowGuiTest, ChatStrgEingabeSendet)  // [ChatDialogTest.md#ChatStrgEingabeSendet]
{
    ChatControls chat = openChat(mainWindow());
    ASSERT_NE(chat.input, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    ASSERT_TRUE(waitUntil([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS));
    ASSERT_TRUE(replaceText(chat.input, "Antworte nur mit dem Wort ja"));

    ASSERT_TRUE(pressControlEnter(chat.input));

    EXPECT_NE(valueOf(chat.transcript).Find("Antworte nur mit dem Wort ja"), wxNOT_FOUND) << "Strg+Eingabe hat die Frage nicht in den Verlauf gestellt.";
    EXPECT_EQ(valueOf(chat.input), "") << "Das Eingabefeld wurde nach dem Senden nicht geleert.";
    EXPECT_TRUE(!chat.send->IsEnabled() || valueOf(chat.transcript).Contains(AFS_Core::translation::translate("error") + L":"))  // [ChatDialogTest.md#no-model]
        << "Der Dialog wartet nach dem Senden nicht auf die Antwort.";
    ASSERT_TRUE(waitUntil([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS)) << "Der Assistent hat nicht geantwortet.";
    EXPECT_TRUE(valueOf(chat.transcript).Contains(AFS_Core::translation::translate("chat_assistant") + L":") || valueOf(chat.transcript).Contains(AFS_Core::translation::translate("error") + L":"));
}

TEST_F(SlowGuiTest, ChatAntwortErscheintWaehrendSieEntsteht)  // [ChatDialogTest.md#ChatAntwortErscheintWaehrendSieEntsteht]
{
    if (!LlamaClient::available())
        GTEST_SKIP() << "Kein lokales Sprachmodell; " << model_location::PATH_VARIABLE << " ist nicht gesetzt.";
    ChatControls chat = openChat(mainWindow());
    ASSERT_NE(chat.input, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    ASSERT_TRUE(waitUntil([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS));
    ASSERT_TRUE(replaceText(chat.input, "Zaehle von 1 bis 40, jede Zahl in einer eigenen Zeile"));
    const wxString speaker = AFS_Core::translation::translate("chat_assistant") + L":";
    const std::size_t answersBefore = countOf(valueOf(chat.transcript), speaker);

    ASSERT_TRUE(pressControlEnter(chat.input));

    ASSERT_TRUE(waitUntil([&] { return countOf(valueOf(chat.transcript), speaker) > answersBefore || chat.send->IsEnabled(); },
        REPLY_TIMEOUT_MILLISECONDS)) << "Der Assistent hat nicht geantwortet.";
    EXPECT_FALSE(chat.send->IsEnabled()) << "Die Antwort erschien erst, als sie fertig war.";
    const wxString early = valueOf(chat.transcript);
    ASSERT_TRUE(waitUntil([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS));
    EXPECT_GT(valueOf(chat.transcript).length(), early.length()) << "Die Antwort wuchs nach dem ersten Stück nicht weiter.";
    EXPECT_EQ(countOf(valueOf(chat.transcript), speaker), answersBefore + 1) << "Die fertige Antwort wurde ein zweites Mal angehängt.";
}

TEST_F(SlowGuiTest, ChatKenntDieAnzahlDerSynchronisationen)  // [ChatDialogTest.md#ChatKenntDieAnzahlDerSynchronisationen]
{
    if (!LlamaClient::available())
        GTEST_SKIP() << "Kein lokales Sprachmodell; " << model_location::PATH_VARIABLE << " ist nicht gesetzt.";
    const wxString source = GuiTestSandbox::sourceDir().string(), target = GuiTestSandbox::targetDir().string();
    ASSERT_TRUE(addSyncItem(mainWindow(), SyncItemInput{"Chat-Eins", source, target}));
    ASSERT_TRUE(addSyncItem(mainWindow(), SyncItemInput{"Chat-Zwei", source, target}));
    ASSERT_TRUE(addSyncItem(mainWindow(), SyncItemInput{"Chat-Drei", source, target}));
    ChatControls chat = openChat(mainWindow());
    ASSERT_NE(chat.input, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    ASSERT_TRUE(waitUntil([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS));
    ASSERT_TRUE(replaceText(chat.input, "Wie viele Synchronisationen sind konfiguriert? Antworte nur mit der Zahl."));
    const wxString speaker = AFS_Core::translation::translate("chat_assistant") + L":";

    ASSERT_TRUE(pressControlEnter(chat.input));

    ASSERT_TRUE(waitUntil([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS)) << "Der Assistent hat nicht geantwortet.";
    const wxString transcript = valueOf(chat.transcript);
    const wxString answer = transcript.Mid(transcript.rfind(speaker));  // [ChatDialogTest.md#last-answer-only]
    EXPECT_NE(answer.Find("3"), wxNOT_FOUND) << "Der Assistent kennt die Anzahl nicht: " << answer;
}

TEST_F(SlowGuiTest, ChatVorschlagLegtSynchronisationUeberDenDialogAn)  // [ChatDialogTest.md#ChatVorschlagLegtSynchronisationUeberDenDialogAn]
{
    if (!LlamaClient::available())
        GTEST_SKIP() << "Kein lokales Sprachmodell; " << model_location::PATH_VARIABLE << " ist nicht gesetzt.";
    const wxString source = GuiTestSandbox::sourceDir().string(), target = GuiTestSandbox::targetDir().string();
    ChatControls chat = openChat(mainWindow());
    ASSERT_NE(chat.input, nullptr) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChat hidden(chat);
    ASSERT_TRUE(waitUntil([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS));
    ASSERT_TRUE(replaceText(chat.input, "Lege eine neue Synchronisation mit dem Namen Chat-Vorschlag an. Quelle: "
        + source + " Ziel: " + target + " Typ: Verzeichnisbaum."));
    wxWindow* create = wxWindow::FindWindowById(ID_CHAT_CREATE_SYNC);
    ASSERT_NE(create, nullptr);
    EXPECT_FALSE(create->IsShown()) << "Der Anlegen-Knopf war schon vor dem Vorschlag sichtbar.";

    ASSERT_TRUE(pressControlEnter(chat.input));
    ASSERT_TRUE(waitUntil([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS)) << "Der Assistent hat nicht geantwortet.";
    const wxString transcript = valueOf(chat.transcript);
    ASSERT_TRUE(create->IsShown()) << "Die Antwort enthielt keinen Vorschlag: " << transcript;
    EXPECT_EQ(transcript.Find("```"), wxNOT_FOUND) << "Der Vorschlagsblock steht im Verlauf.";  // [ChatDialogTest.md#block-not-shown]

    wxString prefilledSource;
    ModalDialogWatcher::instance().expect(txt::add_synchronisation(), [&prefilledSource](wxDialog& dialog) {
        const bool confirmed = waitUntil([&dialog] { return dialog.IsShownOnScreen(); })
            && (prefilledSource = valueOf(find(dialog, ID_SOURCE_DIR_BOX)), true)
            && clickWhenEnabled(find(dialog, wxID_OK))
            && waitUntil([&dialog] { return !dialog.IsShown(); });
        if (!confirmed)
            dialog.EndModal(wxID_CANCEL);
    });
    ASSERT_TRUE(clickOn(create));
    pump();

    EXPECT_EQ(prefilledSource, source) << "Der Dialog war nicht mit der vorgeschlagenen Quelle vorbelegt.";
    const auto item = Config::instance().find_by_name(L"Chat-Vorschlag");
    ASSERT_TRUE(item.has_value()) << "Die Synchronisation kam nicht in die Konfiguration: " << transcript;
    EXPECT_EQ(item->source(), source.ToStdWstring());
    EXPECT_EQ(item->target(), target.ToStdWstring());
    EXPECT_FALSE(create->IsShown()) << "Der Anlegen-Knopf blieb nach dem Anlegen sichtbar.";
}

}
