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
#include "HostKeyPrompt.h"
#include "Synchronize.h"
#include "Translation.h"
#include <wx/app.h>
#include <wx/msgdlg.h>
#include <wx/thread.h>
#include <chrono>
#include <future>
#include <map>
#include <memory>

using AFS_Core::translation::translate;

namespace {

constexpr auto REFUSAL_REMEMBERED = std::chrono::minutes(1);  // [HostKeyPrompt.md#answers-remembered]
constexpr auto ANSWER_POLL = std::chrono::milliseconds(100);
constexpr auto ANSWER_TIMEOUT = std::chrono::minutes(5);  // [HostKeyPrompt.md#worker-threads]

struct Answer {
    bool accepted = false;
    std::chrono::steady_clock::time_point given;
};

bool still_valid(const Answer& answer)
{
    return answer.accepted || std::chrono::steady_clock::now() - answer.given < REFUSAL_REMEMBERED;
}

bool ask(const std::string& host, const std::string& fingerprint)  // [HostKeyPrompt.md#answers-remembered]
{
    static std::map<std::string, Answer> answers;
    const std::string key = host + " " + fingerprint;
    if (const auto known = answers.find(key); known != answers.end() && still_valid(known->second))
        return known->second.accepted;
    const wxString question = translate("host_key_question", {wxString::FromUTF8(host).ToStdWstring(), wxString::FromUTF8(fingerprint).ToStdWstring()});
    const bool accepted = !fingerprint.empty()
        && wxMessageBox(question, translate("host_key_question_title"), wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, wxTheApp->GetTopWindow()) == wxYES;
    answers[key] = {accepted, std::chrono::steady_clock::now()};
    return accepted;
}

bool ask_from_worker(const std::string& host, const std::string& fingerprint)  // [HostKeyPrompt.md#worker-threads]
{
    const auto answer = std::make_shared<std::promise<bool>>();
    std::future<bool> given = answer->get_future();
    wxTheApp->CallAfter([answer, host, fingerprint] { answer->set_value(ask(host, fingerprint)); });
    for (auto waited = std::chrono::milliseconds(0); waited < ANSWER_TIMEOUT; waited += ANSWER_POLL) {
        if (given.wait_for(ANSWER_POLL) == std::future_status::ready)
            return given.get();
        if (AFS_Core::Synchronize::stopping())
            return false;
    }
    return false;
}

}

namespace hostkeyprompt {

bool confirm(const std::string& host, const std::string& fingerprint)
{
    return wxThread::IsMain() ? ask(host, fingerprint) : ask_from_worker(host, fingerprint);
}

}
