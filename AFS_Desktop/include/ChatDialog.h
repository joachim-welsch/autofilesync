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
#pragma once
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <functional>
#include <memory>
#include <mutex>
#include "LlamaClient.h"

using namespace AFS_Core;  // [ChatDialog.md#ChatDialog]

class ChatDialog: public wxDialog {
public:
    ChatDialog(wxWindow* parent, wxWindowID id, const wxString& title);
    ~ChatDialog() override;
private:
    struct Pending {  // [ChatDialog.md#Pending]
        std::mutex  mutex;
        ChatDialog* dialog = nullptr;
    };

    void onSend(wxCommandEvent&);
    void onReply(wxThreadEvent&);
    void onClose(wxCloseEvent&);
    void onInputKey(wxKeyEvent&);
    void appendTurn(const wxString& speaker, const wxString& text);
    void setWaiting(bool waiting);
    void runAsync(std::function<ChatReply()> call);  // [ChatDialog.md#runAsync]

    wxTextCtrl*   transcript_ = nullptr;
    wxTextCtrl*   input_ = nullptr;
    wxButton*     sendButton_ = nullptr;
    wxStaticText* status_ = nullptr;
    ChatHistory   history_;
    std::shared_ptr<LlamaClient> client_ = std::make_shared<LlamaClient>();  // [ChatDialog.md#client_]
    std::shared_ptr<Pending> pending_ = std::make_shared<Pending>();

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_CHAT_INPUT = wxID_HIGHEST + 200,
    ID_CHAT_SEND,
    ID_CHAT_REPLY,
    ID_CHAT_TRANSCRIPT  // [ChatDialog.md#ID_CHAT_TRANSCRIPT]
};
