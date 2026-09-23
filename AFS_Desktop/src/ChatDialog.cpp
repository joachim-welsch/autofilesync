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
#include "ChatDialog.h"
#include <thread>
#include "LogAnalysis.h"

using namespace AFS_Core;

wxBEGIN_EVENT_TABLE(ChatDialog, wxDialog)
    EVT_BUTTON(ID_CHAT_SEND, ChatDialog::onSend)
    EVT_THREAD(ID_CHAT_REPLY, ChatDialog::onReply)
    EVT_CLOSE(ChatDialog::onClose)
wxEND_EVENT_TABLE()

namespace {
constexpr std::size_t maxLogExcerptChars = 500;  // [ChatDialog.md#maxLogExcerptChars]
}

ChatDialog::ChatDialog(wxWindow* parent, wxWindowID id, const wxString& title)
    : wxDialog(parent, id, title, wxDefaultPosition, wxSize(600, 500),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    pending_->dialog = this;
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    transcript_ = new wxTextCtrl(this, ID_CHAT_TRANSCRIPT, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                 wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxTE_BESTWRAP);
    mainSizer->Add(transcript_, 1, wxEXPAND | wxALL, 5);

    input_ = new wxTextCtrl(this, ID_CHAT_INPUT, wxEmptyString, wxDefaultPosition, wxSize(-1, 70),
                            wxTE_MULTILINE | wxTE_BESTWRAP);
    mainSizer->Add(input_, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

    wxBoxSizer* bottomSizer = new wxBoxSizer(wxHORIZONTAL);
    status_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    bottomSizer->Add(status_, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
    sendButton_ = new wxButton(this, ID_CHAT_SEND, _("Senden"));
    bottomSizer->Add(sendButton_, 0, wxALL, 5);
    mainSizer->Add(bottomSizer, 0, wxEXPAND);

    SetSizer(mainSizer);
    Centre();
    input_->Bind(wxEVT_KEY_DOWN, &ChatDialog::onInputKey, this);  // [ChatDialog.md#key-handler-on-control]
    if (!LlamaClient::available()) {
        appendTurn(_("Hinweis"), LlamaClient::missing_model_hint());  // [ChatDialog.md#hint-in-transcript]
        status_->SetLabel(_(L"Kein Modell gefunden."));
    }
    else
        status_->SetLabel(_(L"Strg+Eingabe sendet."));
    input_->CallAfter(&wxTextCtrl::SetFocus);  // [ChatDialog.md#deferred-focus]
}

ChatDialog::~ChatDialog()
{
    std::lock_guard<std::mutex> lock(pending_->mutex);
    pending_->dialog = nullptr;
}

void ChatDialog::appendTurn(const wxString& speaker, const wxString& text)
{
    if (!transcript_->IsEmpty())
        transcript_->AppendText("\n");
    transcript_->AppendText(speaker + ":\n" + text + "\n");
    transcript_->ShowPosition(transcript_->GetLastPosition());
}

void ChatDialog::setWaiting(bool waiting)
{
    sendButton_->Enable(!waiting);
    input_->SetEditable(!waiting);
    status_->SetLabel(waiting ? _(L"Der Assistent antwortet…") : _(L"Strg+Eingabe sendet."));
}

void ChatDialog::runAsync(std::function<ChatReply()> call)
{
    setWaiting(true);
    std::thread([call = std::move(call), pending = pending_]() {
        const ChatReply reply = call();
        std::lock_guard<std::mutex> lock(pending->mutex);
        if (pending->dialog == nullptr)
            return;
        wxThreadEvent* event = new wxThreadEvent(wxEVT_THREAD, ID_CHAT_REPLY);
        event->SetInt(reply.ok ? 1 : 0);
        event->SetString(reply.text);
        wxQueueEvent(pending->dialog, event);
    }).detach();
}

void ChatDialog::onSend(wxCommandEvent&)
{
    const wxString question = input_->GetValue().Trim().Trim(false);
    if (question.IsEmpty())
        return;
    input_->Clear();
    appendTurn(_("Du"), question);
    history_.push_back({ true, question.ToStdWstring() });
    const ChatHistory snapshot = history_;  // [ChatDialog.md#history-snapshot]
    if (looks_like_log_question(question.ToStdWstring())) {  // [ChatDialog.md#log-excerpt-per-request]
        const std::wstring excerpt = read_recent_log_entries(maxLogExcerptChars);
        runAsync([client = client_, snapshot, excerpt]() { return client->analyze_log(snapshot, excerpt); });
    }
    else
        runAsync([client = client_, snapshot]() { return client->send(snapshot); });
}

void ChatDialog::onReply(wxThreadEvent& event)
{
    setWaiting(false);
    const wxString text = event.GetString();
    if (event.GetInt() == 1) {
        appendTurn(_("Assistent"), text);
        history_.push_back({ false, text.ToStdWstring() });  // [ChatDialog.md#only-answers-in-history]
    }
    else
        appendTurn(_("Fehler"), text);
    input_->SetFocus();
}

void ChatDialog::onInputKey(wxKeyEvent& event)
{
    if (event.GetKeyCode() == WXK_RETURN && event.ControlDown() && sendButton_->IsEnabled()) {  // [ChatDialog.md#ctrl-enter-sends]
        wxCommandEvent unused;
        onSend(unused);
        return;
    }
    event.Skip();
}

void ChatDialog::onClose(wxCloseEvent&)
{
    Hide();  // [ChatDialog.md#hidden-not-destroyed]
}
