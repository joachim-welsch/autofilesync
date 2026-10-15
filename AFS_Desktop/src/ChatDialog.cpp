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
    EVT_BUTTON(ID_CHAT_SEND, ChatDialog::on_send)
    EVT_BUTTON(ID_CHAT_PROPOSAL, ChatDialog::on_proposal)
    EVT_BUTTON(ID_CHAT_NEW, ChatDialog::on_new_conversation)
    EVT_THREAD(ID_CHAT_PIECE, ChatDialog::on_piece)
    EVT_THREAD(ID_CHAT_REPLY, ChatDialog::on_reply)
    EVT_CLOSE(ChatDialog::on_close)
wxEND_EVENT_TABLE()

namespace {
#ifdef __WXOSX__
constexpr const char* SEND_HINT = "chat_cmd_enter";  // [ChatDialog.md#ctrl-enter-sends]
#else
constexpr const char* SEND_HINT = "chat_ctrl_enter";
#endif
constexpr std::size_t MAX_LOG_EXCERPT_CHARS = 500;  // [ChatDialog.md#MAX_LOG_EXCERPT_CHARS]

wxString answer_with_proposal(const std::wstring& answer, const sync_proposal::Proposal& proposal)  // [ChatDialog.md#proposal-described]
{
    const std::wstring prose = sync_proposal::without_block(answer);
    return prose.empty() ? sync_proposal::describe(proposal) : prose + L"\n" + sync_proposal::describe(proposal);
}
}

ChatDialog::ChatDialog(wxWindow* parent, wxWindowID id, const wxString& title, ApplyProposal apply_proposal)
    : wxDialog(parent, id, title, wxDefaultPosition, FromDIP(wxSize(600, 500), parent),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , apply_proposal_(std::move(apply_proposal))
{
    pending_->dialog = this;
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    transcript_ = new wxTextCtrl(this, ID_CHAT_TRANSCRIPT, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                 wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxTE_BESTWRAP);
    main_sizer->Add(transcript_, 1, wxEXPAND | wxALL, 5);

    input_ = new wxTextCtrl(this, ID_CHAT_INPUT, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(-1, 70)),
                            wxTE_MULTILINE | wxTE_BESTWRAP);
    main_sizer->Add(input_, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

    wxBoxSizer* bottom_sizer = new wxBoxSizer(wxHORIZONTAL);
    status_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    bottom_sizer->Add(status_, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
    new_button_ = new wxButton(this, ID_CHAT_NEW, translation::translate("chat_new_conversation"));
    bottom_sizer->Add(new_button_, 0, wxALL, 5);
    proposal_button_ = new wxButton(this, ID_CHAT_PROPOSAL, translation::translate("chat_create_sync"));
    proposal_button_->Hide();  // [ChatDialog.md#proposal-button]
    bottom_sizer->Add(proposal_button_, 0, wxALL, 5);
    send_button_ = new wxButton(this, ID_CHAT_SEND, translation::translate("button_send"));
    bottom_sizer->Add(send_button_, 0, wxALL, 5);
    main_sizer->Add(bottom_sizer, 0, wxEXPAND);

    SetSizer(main_sizer);
    Centre();
    input_->Bind(wxEVT_KEY_DOWN, &ChatDialog::on_input_key, this);  // [ChatDialog.md#key-handler-on-control]
    show_start();
    input_->CallAfter(&wxTextCtrl::SetFocus);  // [ChatDialog.md#deferred-focus]
}

void ChatDialog::show_start()
{
    if (!LlamaClient::available()) {
        append_turn(translation::translate("chat_hint"), LlamaClient::missing_model_hint());  // [ChatDialog.md#hint-in-transcript]
        status_->SetLabel(translation::translate("chat_no_model"));
    }
    else
        status_->SetLabel(translation::translate(SEND_HINT));
}

ChatDialog::~ChatDialog()
{
    std::lock_guard<std::mutex> lock(pending_->mutex);
    pending_->dialog = nullptr;
}

void ChatDialog::append_speaker(const wxString& speaker)
{
    if (!transcript_->IsEmpty())
        transcript_->AppendText("\n");
    transcript_->AppendText(speaker + ":\n");
}

void ChatDialog::append_turn(const wxString& speaker, const wxString& text)
{
    append_speaker(speaker);
    transcript_->AppendText(text + "\n");
    transcript_->ShowPosition(transcript_->GetLastPosition());
}

void ChatDialog::show_streamed(const wxString& piece)
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

void ChatDialog::replace_streamed_turn(const wxString& text)  // [ChatDialog.md#replace_streamed_turn]
{
    transcript_->Remove(turn_start_, transcript_->GetLastPosition());
    transcript_->AppendText(text + "\n");
    transcript_->ShowPosition(transcript_->GetLastPosition());
    answer_streaming_ = false;
}

void ChatDialog::show_proposal(const std::optional<sync_proposal::Proposal>& proposal)
{
    proposal_ = proposal;
    const bool deletion = proposal_ && std::holds_alternative<sync_proposal::Deletion>(*proposal_);
    proposal_button_->SetLabel(translation::translate(deletion ? "chat_delete_sync" : "chat_create_sync"));
    proposal_button_->Show(proposal_.has_value());
    Layout();
}

void ChatDialog::set_waiting(bool waiting)
{
    send_button_->Enable(!waiting);
    new_button_->Enable(!waiting);  // [ChatDialog.md#on_new_conversation]
    input_->SetEditable(!waiting);
    status_->SetLabel(waiting ? translation::translate("chat_answering") : translation::translate(SEND_HINT));
}

void ChatDialog::run_async(std::function<ChatReply(const ChatPieceSink&)> call)
{
    set_waiting(true);
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

void ChatDialog::on_send(wxCommandEvent&)
{
    const wxString question = input_->GetValue().Trim().Trim(false);
    if (question.IsEmpty())
        return;
    input_->Clear();
    show_proposal(std::nullopt);  // [ChatDialog.md#proposal-button]
    streamed_.Clear();
    shown_ = 0;
    append_turn(translation::translate("chat_you"), question);
    history_.push_back({ true, question.ToStdWstring() });
    const ChatHistory snapshot = history_;  // [ChatDialog.md#history-snapshot]
    if (looks_like_log_question(question.ToStdWstring())) {  // [ChatDialog.md#log-excerpt-per-request]
        const std::wstring excerpt = read_recent_log_entries(MAX_LOG_EXCERPT_CHARS);
        run_async([client = client_, snapshot, excerpt](const ChatPieceSink& on_piece) {
            return client->analyze_log(snapshot, excerpt, on_piece);
        });
    }
    else
        run_async([client = client_, snapshot](const ChatPieceSink& on_piece) { return client->send(snapshot, on_piece); });
}

void ChatDialog::on_piece(wxThreadEvent& event)
{
    if (!answer_streaming_) {
        append_speaker(translation::translate("chat_assistant"));
        turn_start_ = transcript_->GetLastPosition();
        answer_streaming_ = true;
    }
    show_streamed(event.GetString());
}

void ChatDialog::on_reply(wxThreadEvent& event)
{
    set_waiting(false);
    const wxString text = event.GetString();
    const bool ok = event.GetInt() == 1;
    const std::optional<sync_proposal::Proposal> proposal = ok ? sync_proposal::parse(text.ToStdWstring()) : std::nullopt;
    const wxString shown = proposal ? answer_with_proposal(text.ToStdWstring(), *proposal) : text;
    if (answer_streaming_)
        replace_streamed_turn(shown);
    else
        append_turn(translation::translate(ok ? "chat_assistant" : "error"), shown);
    if (ok)
        history_.push_back({ false, text.ToStdWstring() });  // [ChatDialog.md#only-answers-in-history]
    show_proposal(proposal);
    if (IsShown())  // [ChatDialog.md#focus-only-when-shown]
        input_->SetFocus();
}

void ChatDialog::on_proposal(wxCommandEvent&)
{
    if (!proposal_)
        return;
    const sync_proposal::Proposal proposal = *proposal_;
    show_proposal(std::nullopt);
    apply_proposal_(proposal);
}

void ChatDialog::on_new_conversation(wxCommandEvent&)
{
    history_.clear();
    transcript_->Clear();
    show_proposal(std::nullopt);
    show_start();
    input_->SetFocus();
}

void ChatDialog::on_input_key(wxKeyEvent& event)
{
    if (event.GetKeyCode() == WXK_RETURN && event.ControlDown() && send_button_->IsEnabled()) {  // [ChatDialog.md#ctrl-enter-sends]
        wxCommandEvent unused;
        on_send(unused);
        return;
    }
    event.Skip();
}

void ChatDialog::on_close(wxCloseEvent&)
{
    Hide();  // [ChatDialog.md#hidden-not-destroyed]
}
