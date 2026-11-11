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
#include "id.h"
#include "utilwx.h"
#include "Logging.h"
#include "Translation.h"
#include "Util.h"
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace AFS_Core;

using utilwx::tul;
using util::wstring_to_string;

namespace guitest {
namespace {
constexpr int REPLY_TIMEOUT_MILLISECONDS = 900000;  // [ChatLogQuestionTest.md#wait-for-the-answer]
const wxString LOG_QUESTION = "Gibt es Warnings im Log?";

void write_sample_log_entries(const std::string& path)  // [ChatLogQuestionTest.md#write_sample_log_entries]
{
    std::ofstream out(path, std::ios::app);
    constexpr std::size_t MIN_BYTES = 4000; // comfortably above MAX_LOG_EXCERPT_CHARS
    std::size_t written = 0;
    for (int i = 0; written < MIN_BYTES; ++i) {
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
    std::string original_content_;
public:
    explicit ScopedSampleLogEntries(std::string path)
        : path_(std::move(path))
    {
        std::ostringstream content;
        content << std::ifstream(path_).rdbuf();
        original_content_ = content.str();
        write_sample_log_entries(path_);
    }
    ~ScopedSampleLogEntries()
    {
        std::ofstream(path_, std::ios::trunc) << original_content_;
    }
};

class HiddenChatDialog {  // [ChatLogQuestionTest.md#hidden-not-closed]
public:
    explicit HiddenChatDialog(wxWindow* input): dialog_(input != nullptr ? wxGetTopLevelParent(input) : nullptr) {}
    ~HiddenChatDialog()
    {
        if (dialog_ != nullptr)
            dialog_->Hide();
    }
private:
    wxWindow* dialog_;
};
}

TEST_F(DISABLED_SlowGuiTest, LogFrageWirdImChatBeantwortet)  // [ChatLogQuestionTest.md#LogFrageWirdImChatBeantwortet]
{
    const ScopedSampleLogEntries sample_log(wstring_to_string(Logging::instance().get_log_file_path_name()));

    activate_menu_item(main_window(), tul(id::Chat));

    wxWindow* input = nullptr;  // [ChatLogQuestionTest.md#dialog-is-a-separate-window]
    ASSERT_TRUE(wait_until([&input] {
        input = wxWindow::FindWindowById(ID_CHAT_INPUT);
        return input != nullptr && input->IsShownOnScreen();
    })) << "Der Chat-Dialog wurde nicht geöffnet.";
    const HiddenChatDialog hidden(input);
    wxWindow* send_button = wxWindow::FindWindowById(ID_CHAT_SEND);
    wxWindow* transcript = wxWindow::FindWindowById(ID_CHAT_TRANSCRIPT);
    ASSERT_NE(send_button, nullptr);
    ASSERT_NE(transcript, nullptr);

    ASSERT_TRUE(type_into(input, LOG_QUESTION));
    ASSERT_TRUE(click_when_enabled(send_button));

    ASSERT_TRUE(wait_until([send_button] { return send_button->IsEnabled(); }, REPLY_TIMEOUT_MILLISECONDS))  // [ChatLogQuestionTest.md#wait-for-the-answer]
        << "Der Assistent hat nicht geantwortet.";

    const wxString transcript_text = value_of(transcript);
    EXPECT_NE(transcript_text.Find(LOG_QUESTION), wxNOT_FOUND) << "Die Frage fehlt im Verlauf.";
    EXPECT_TRUE(transcript_text.Contains(translation::translate("chat_assistant") + L":") || transcript_text.Contains(translation::translate("error") + L":"))
        << "Es kam keine Antwort im Verlauf an.";
}

}
