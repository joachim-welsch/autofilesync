#pragma once
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <functional>
#include <memory>
#include <mutex>
#include "LlamaClient.h"

// Chat with the assistant. Modeless, so a long answer never blocks the rest of
// the program, and hidden instead of destroyed on close so the conversation
// survives until the program ends.

using namespace AFS_Core;

class ChatDialog: public wxDialog {
public:
    ChatDialog(wxWindow* parent, wxWindowID id, const wxString& title);
    ~ChatDialog() override;
private:
    // Shared with the worker thread. The worker may only reach the dialog while
    // it holds the lock and dialog_ is still set; the destructor clears it under
    // the same lock, so an answer arriving after the dialog is gone is dropped
    // instead of being posted to a destroyed window.
    struct Pending {
        std::mutex  mutex;
        ChatDialog* dialog = nullptr;
    };

    void onSend(wxCommandEvent&);
    void onReply(wxThreadEvent&);
    void onClose(wxCloseEvent&);
    void onInputKey(wxKeyEvent&);
    void appendTurn(const wxString& speaker, const wxString& text);
    void setWaiting(bool waiting);
    // Runs call() on a worker thread and posts its ChatReply back to onReply().
    void runAsync(std::function<ChatReply()> call);

    wxTextCtrl*   transcript_ = nullptr;
    wxTextCtrl*   input_ = nullptr;
    wxButton*     sendButton_ = nullptr;
    wxStaticText* status_ = nullptr;
    ChatHistory   history_;
    // Shared rather than a plain member: the worker thread needs the loaded
    // model to stay alive for the send() call, and LlamaClient's llama_model/
    // llama_context members are non-copyable.
    std::shared_ptr<LlamaClient> client_ = std::make_shared<LlamaClient>();
    std::shared_ptr<Pending> pending_ = std::make_shared<Pending>();

    wxDECLARE_EVENT_TABLE();
};

enum {
    ID_CHAT_INPUT = wxID_HIGHEST + 200,
    ID_CHAT_SEND,
    ID_CHAT_REPLY,
    // Stable id for the read-only transcript, otherwise wxID_ANY: GUI tests
    // need it to read the conversation back via FindWindowById().
    ID_CHAT_TRANSCRIPT
};
