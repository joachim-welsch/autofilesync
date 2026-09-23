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
#include "Logging.h"
#include "Util.h"
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace AFS_Core;

using utilwx::tul;
using util::wstring_to_string;

namespace guitest {
namespace {
const wxString logQuestion = "Gibt es Warnings im Log?";

void writeSampleLogEntries(const std::string& path)  // [ChatLogQuestionTest.md#writeSampleLogEntries]
{
    std::ofstream out(path, std::ios::app);
    constexpr std::size_t minBytes = 4000; // comfortably above maxLogExcerptChars
    std::size_t written = 0;
    for (int i = 0; written < minBytes; ++i) {
        std::ostringstream line;
        line << "2026-08-12 16:35:" << std::setw(2) << std::setfill('0') << (i % 60)
             << ".415 Warning\tZielverzeichnis fast voll (Eintrag " << i
             << "): pi_Spoccia:/home/pi/mirror\n";
        out << line.str();
        written += line.str().size();
    }
}

class ScopedSampleLogEntries {  // [ChatLogQuestionTest.md#ScopedSampleLogEntries]
    std::string path_;
    std::string originalContent_;
public:
    explicit ScopedSampleLogEntries(std::string path)
        : path_(std::move(path))
    {
        std::ostringstream content;
        content << std::ifstream(path_).rdbuf();
        originalContent_ = content.str();
        writeSampleLogEntries(path_);
    }
    ~ScopedSampleLogEntries()
    {
        std::ofstream(path_, std::ios::trunc) << originalContent_;
    }
};
}

TEST_F(SlowGuiTest, LogFrageWirdImChatBeantwortet)  // [ChatLogQuestionTest.md#LogFrageWirdImChatBeantwortet]
{
    const ScopedSampleLogEntries sampleLog(wstring_to_string(Logging::instance().get_log_file_path_name()));

    activateMenuItem(mainWindow(), tul(id::Chat));

    wxWindow* input = nullptr;  // [ChatLogQuestionTest.md#dialog-is-a-separate-window]
    ASSERT_TRUE(waitUntil([&input] {
        input = wxWindow::FindWindowById(ID_CHAT_INPUT);
        return input != nullptr && input->IsShownOnScreen();
    })) << "Der Chat-Dialog wurde nicht geöffnet.";
    wxWindow* sendButton = wxWindow::FindWindowById(ID_CHAT_SEND);
    wxWindow* transcript = wxWindow::FindWindowById(ID_CHAT_TRANSCRIPT);
    ASSERT_NE(sendButton, nullptr);
    ASSERT_NE(transcript, nullptr);

    ASSERT_TRUE(typeInto(input, logQuestion));
    ASSERT_TRUE(clickWhenEnabled(sendButton));

    ASSERT_TRUE(waitUntil([sendButton] { return sendButton->IsEnabled(); }, 240000))  // [ChatLogQuestionTest.md#wait-for-the-answer]
        << "Der Assistent hat nicht geantwortet.";

    const wxString transcriptText = valueOf(transcript);
    EXPECT_NE(transcriptText.Find(logQuestion), wxNOT_FOUND) << "Die Frage fehlt im Verlauf.";
    EXPECT_TRUE(transcriptText.Contains("Assistent:") || transcriptText.Contains("Fehler:"))
        << "Es kam keine Antwort im Verlauf an.";

    wxDialog* chatDialog = dynamic_cast<wxDialog*>(input->GetParent());  // [ChatLogQuestionTest.md#hidden-not-closed]
    if (chatDialog != nullptr)
        chatDialog->Hide();
}

}
