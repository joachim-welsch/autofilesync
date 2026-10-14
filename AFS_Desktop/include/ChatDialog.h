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
#pragma once
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include "LlamaClient.h"
#include "SyncProposal.h"

using namespace AFS_Core;  // [ChatDialog.md#ChatDialog]

class ChatDialog: public wxDialog {
public:
    using ApplyProposal = std::function<void(const sync_proposal::Proposal& proposal)>;  // [ChatDialog.md#ApplyProposal]
    ChatDialog(wxWindow* parent, wxWindowID id, const wxString& title, ApplyProposal apply_proposal);
    ~ChatDialog() override;
private:
    struct Pending {  // [ChatDialog.md#Pending]
        std::mutex  mutex;
        ChatDialog* dialog = nullptr;
    };

    void on_send(wxCommandEvent&);
    void on_proposal(wxCommandEvent&);
    void on_piece(wxThreadEvent&);
    void on_reply(wxThreadEvent&);
    void on_close(wxCloseEvent&);
    void on_input_key(wxKeyEvent&);
    void append_speaker(const wxString& speaker);
    void append_turn(const wxString& speaker, const wxString& text);
    void show_streamed(const wxString& piece);
    void replace_streamed_turn(const wxString& text);
    void show_proposal(const std::optional<sync_proposal::Proposal>& proposal);
    void set_waiting(bool waiting);
    void run_async(std::function<ChatReply(const ChatPieceSink&)> call);  // [ChatDialog.md#run_async]

    wxTextCtrl*   transcript_ = nullptr;
    wxTextCtrl*   input_ = nullptr;
    wxButton*     send_button_ = nullptr;
    wxButton*     proposal_button_ = nullptr;  // [ChatDialog.md#proposal-button]
    wxStaticText* status_ = nullptr;
    ChatHistory   history_;
    bool          answer_streaming_ = false;  // [ChatDialog.md#answer_streaming_]
    long          turn_start_ = 0;
    wxString      streamed_;
    std::size_t   shown_ = 0;  // [ChatDialog.md#block-held-back]
    std::optional<sync_proposal::Proposal> proposal_;
    ApplyProposal apply_proposal_;
    std::shared_ptr<LlamaClient> client_ = std::make_shared<LlamaClient>();  // [ChatDialog.md#client_]
    std::shared_ptr<Pending> pending_ = std::make_shared<Pending>();

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_CHAT_INPUT = wxID_HIGHEST + 200,
    ID_CHAT_SEND,
    ID_CHAT_REPLY,
    ID_CHAT_PIECE,
    ID_CHAT_PROPOSAL,
    ID_CHAT_TRANSCRIPT  // [ChatDialog.md#ID_CHAT_TRANSCRIPT]
};
