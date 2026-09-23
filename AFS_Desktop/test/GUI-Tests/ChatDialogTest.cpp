/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-AutoFileSync-Commercial
 *
 * This file is part of AFS_Desktop, the desktop program of AutoFileSync.
 *
 * AutoFileSync is dual-licensed: you may use, modify and redistribute it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License or (at your option) any later
 * version, or under the AutoFileSync Commercial Licence obtained from the
 * copyright holder. See LICENSING.md, LICENSE and LICENSE-COMMERCIAL.md in the
 * root of the source tree.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the applicable licence for more details.
 */
#include "GuiTest.h"
#include "EventPump.h"
#include "UserActions.h"
#include "ChatDialog.h"
#include "id.h"
#include "utilwx.h"
#include <wx/uiaction.h>

using utilwx::tul;

namespace guitest {
namespace {

constexpr int REPLY_TIMEOUT_MILLISECONDS = 240000;  // the local language model answers in one to two minutes on a CPU

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

bool pressControlEnter(wxWindow* input)
{
    if (!clickOn(input))
        return false;
    wxUIActionSimulator simulator;
    const bool sent = simulator.Char(WXK_RETURN, wxMOD_CONTROL);
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
    EXPECT_TRUE(!chat.send->IsEnabled() || valueOf(chat.transcript).Contains("Fehler:"))  // [ChatDialogTest.md#no-model]
        << "Der Dialog wartet nach dem Senden nicht auf die Antwort.";
    ASSERT_TRUE(waitUntil([&chat] { return chat.send->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS)) << "Der Assistent hat nicht geantwortet.";
    EXPECT_TRUE(valueOf(chat.transcript).Contains("Assistent:") || valueOf(chat.transcript).Contains("Fehler:"));
}

}
