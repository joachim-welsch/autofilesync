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
#include "ChatDialog.h"
#include <thread>
#include "LogAnalysis.h"
#include "SyncProposal.h"
#include "Translation.h"

using namespace AFS_Core;

wxBEGIN_EVENT_TABLE(ChatDialog, wxDialog)
    EVT_BUTTON(ID_CHAT_SEND, ChatDialog::onSend)
    EVT_BUTTON(ID_CHAT_PROPOSAL, ChatDialog::onProposal)
    EVT_THREAD(ID_CHAT_PIECE, ChatDialog::onPiece)
    EVT_THREAD(ID_CHAT_REPLY, ChatDialog::onReply)
    EVT_CLOSE(ChatDialog::onClose)
wxEND_EVENT_TABLE()

namespace {
#ifdef __WXOSX__
constexpr const char* SEND_HINT = "chat_cmd_enter";  // [ChatDialog.md#ctrl-enter-sends]
#else
constexpr const char* SEND_HINT = "chat_ctrl_enter";
#endif
constexpr std::size_t maxLogExcerptChars = 500;  // [ChatDialog.md#maxLogExcerptChars]

wxString answerWithProposal(const std::wstring& answer, const sync_proposal::Proposal& proposal)  // [ChatDialog.md#proposal-described]
{
    const std::wstring prose = sync_proposal::without_block(answer);
    return prose.empty() ? sync_proposal::describe(proposal) : prose + L"\n" + sync_proposal::describe(proposal);
}
}

ChatDialog::ChatDialog(wxWindow* parent, wxWindowID id, const wxString& title, ApplyProposal applyProposal)
    : wxDialog(parent, id, title, wxDefaultPosition, FromDIP(wxSize(600, 500), parent),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , applyProposal_(std::move(applyProposal))
{
    pending_->dialog = this;
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    transcript_ = new wxTextCtrl(this, ID_CHAT_TRANSCRIPT, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                 wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxTE_BESTWRAP);
    mainSizer->Add(transcript_, 1, wxEXPAND | wxALL, 5);

    input_ = new wxTextCtrl(this, ID_CHAT_INPUT, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(-1, 70)),
                            wxTE_MULTILINE | wxTE_BESTWRAP);
    mainSizer->Add(input_, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

    wxBoxSizer* bottomSizer = new wxBoxSizer(wxHORIZONTAL);
    status_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    bottomSizer->Add(status_, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
    proposalButton_ = new wxButton(this, ID_CHAT_PROPOSAL, translation::translate("chat_create_sync"));
    proposalButton_->Hide();  // [ChatDialog.md#proposal-button]
    bottomSizer->Add(proposalButton_, 0, wxALL, 5);
    sendButton_ = new wxButton(this, ID_CHAT_SEND, translation::translate("button_send"));
    bottomSizer->Add(sendButton_, 0, wxALL, 5);
    mainSizer->Add(bottomSizer, 0, wxEXPAND);

    SetSizer(mainSizer);
    Centre();
    input_->Bind(wxEVT_KEY_DOWN, &ChatDialog::onInputKey, this);  // [ChatDialog.md#key-handler-on-control]
    if (!LlamaClient::available()) {
        appendTurn(translation::translate("chat_hint"), LlamaClient::missing_model_hint());  // [ChatDialog.md#hint-in-transcript]
        status_->SetLabel(translation::translate("chat_no_model"));
    }
    else
        status_->SetLabel(translation::translate(SEND_HINT));
    input_->CallAfter(&wxTextCtrl::SetFocus);  // [ChatDialog.md#deferred-focus]
}

ChatDialog::~ChatDialog()
{
    std::lock_guard<std::mutex> lock(pending_->mutex);
    pending_->dialog = nullptr;
}

void ChatDialog::appendSpeaker(const wxString& speaker)
{
    if (!transcript_->IsEmpty())
        transcript_->AppendText("\n");
    transcript_->AppendText(speaker + ":\n");
}

void ChatDialog::appendTurn(const wxString& speaker, const wxString& text)
{
    appendSpeaker(speaker);
    transcript_->AppendText(text + "\n");
    transcript_->ShowPosition(transcript_->GetLastPosition());
}

void ChatDialog::showStreamed(const wxString& piece)
{
    streamed_ += piece;
    const int fence = streamed_.Find("```");  // [ChatDialog.md#block-held-back]
    const std::size_t visible = fence == wxNOT_FOUND ? streamed_.length() : static_cast<std::size_t>(fence);
    if (visible <= shown_)
        return;
    transcript_->AppendText(streamed_.Mid(shown_, visible - shown_));
    transcript_->ShowPosition(transcript_->GetLastPosition());
    shown_ = visible;
}

void ChatDialog::replaceStreamedTurn(const wxString& text)  // [ChatDialog.md#replaceStreamedTurn]
{
    transcript_->Remove(turnStart_, transcript_->GetLastPosition());
    transcript_->AppendText(text + "\n");
    transcript_->ShowPosition(transcript_->GetLastPosition());
    answerStreaming_ = false;
}

void ChatDialog::showProposal(const std::optional<sync_proposal::Proposal>& proposal)
{
    proposal_ = proposal;
    const bool deletion = proposal_ && std::holds_alternative<sync_proposal::Deletion>(*proposal_);
    proposalButton_->SetLabel(translation::translate(deletion ? "chat_delete_sync" : "chat_create_sync"));
    proposalButton_->Show(proposal_.has_value());
    Layout();
}

void ChatDialog::setWaiting(bool waiting)
{
    sendButton_->Enable(!waiting);
    input_->SetEditable(!waiting);
    status_->SetLabel(waiting ? translation::translate("chat_answering") : translation::translate(SEND_HINT));
}

void ChatDialog::runAsync(std::function<ChatReply(const ChatPieceSink&)> call)
{
    setWaiting(true);
    std::thread([call = std::move(call), pending = pending_]() {
        const ChatReply reply = call([&pending](const std::wstring& piece) {
            std::lock_guard<std::mutex> lock(pending->mutex);
            if (pending->dialog == nullptr)
                return;
            wxThreadEvent* event = new wxThreadEvent(wxEVT_THREAD, ID_CHAT_PIECE);
            event->SetString(piece);
            wxQueueEvent(pending->dialog, event);  // [ChatDialog.md#pieces-before-reply]
        });
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
    showProposal(std::nullopt);  // [ChatDialog.md#proposal-button]
    streamed_.Clear();
    shown_ = 0;
    appendTurn(translation::translate("chat_you"), question);
    history_.push_back({ true, question.ToStdWstring() });
    const ChatHistory snapshot = history_;  // [ChatDialog.md#history-snapshot]
    if (looks_like_log_question(question.ToStdWstring())) {  // [ChatDialog.md#log-excerpt-per-request]
        const std::wstring excerpt = read_recent_log_entries(maxLogExcerptChars);
        runAsync([client = client_, snapshot, excerpt](const ChatPieceSink& on_piece) {
            return client->analyze_log(snapshot, excerpt, on_piece);
        });
    }
    else
        runAsync([client = client_, snapshot](const ChatPieceSink& on_piece) { return client->send(snapshot, on_piece); });
}

void ChatDialog::onPiece(wxThreadEvent& event)
{
    if (!answerStreaming_) {
        appendSpeaker(translation::translate("chat_assistant"));
        turnStart_ = transcript_->GetLastPosition();
        answerStreaming_ = true;
    }
    showStreamed(event.GetString());
}

void ChatDialog::onReply(wxThreadEvent& event)
{
    setWaiting(false);
    const wxString text = event.GetString();
    const bool ok = event.GetInt() == 1;
    const std::optional<sync_proposal::Proposal> proposal = ok ? sync_proposal::parse(text.ToStdWstring()) : std::nullopt;
    const wxString shown = proposal ? answerWithProposal(text.ToStdWstring(), *proposal) : text;
    if (answerStreaming_)
        replaceStreamedTurn(shown);
    else
        appendTurn(translation::translate(ok ? "chat_assistant" : "error"), shown);
    if (ok)
        history_.push_back({ false, text.ToStdWstring() });  // [ChatDialog.md#only-answers-in-history]
    showProposal(proposal);
    if (IsShown())  // [ChatDialog.md#focus-only-when-shown]
        input_->SetFocus();
}

void ChatDialog::onProposal(wxCommandEvent&)
{
    if (!proposal_)
        return;
    const sync_proposal::Proposal proposal = *proposal_;
    showProposal(std::nullopt);
    applyProposal_(proposal);
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
