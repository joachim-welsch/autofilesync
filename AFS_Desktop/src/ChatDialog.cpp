#include "ChatDialog.h"
#include <thread>
#include "LogAnalysis.h"

wxBEGIN_EVENT_TABLE(ChatDialog, wxDialog)
    EVT_BUTTON(ID_CHAT_SEND, ChatDialog::onSend)
    EVT_THREAD(ID_CHAT_REPLY, ChatDialog::onReply)
    EVT_CLOSE(ChatDialog::onClose)
wxEND_EVENT_TABLE()

namespace {
// Bounded by response time, not by n_batch/n_ctx: fitting inside the token
// budget (see LlamaClient::ensureLoaded()) isn't enough on its own - CPU
// prompt processing of a several-thousand-token excerpt measured minutes,
// not seconds, on ordinary hardware (see the LogFrageWirdImChatBeantwortet
// GUI test, which used to time out at 3000 chars). 500 chars measured well
// under a minute for a real answer; kept small deliberately since larger
// values were not just slower but disproportionately slower.
constexpr std::size_t maxLogExcerptChars = 500;
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
    // A key handler on the control, not in the event table: the table belongs to
    // the dialog and never sees the keys the text field consumes.
    input_->Bind(wxEVT_KEY_DOWN, &ChatDialog::onInputKey, this);
    if (!LlamaClient::available())
        status_->SetLabel(_(L"Kein Modell konfiguriert (AUTOFILESYNC_LLAMA_MODEL)."));
    else
        status_->SetLabel(_(L"Strg+Eingabe sendet."));
    // Deferred: the dialog is not shown yet at construction time (MainWindow::onChat
    // creates it, then calls Show()), and a widget that is not yet realised cannot
    // reliably take the X input focus - the very first keystroke could go nowhere.
    input_->CallAfter(&wxTextCtrl::SetFocus);
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
    // The history is copied for the async call: the dialog keeps accepting
    // input while the request runs, so the member must not be read from there on.
    const ChatHistory snapshot = history_;
    // A question about the log gets the current excerpt folded into just this
    // request (see LlamaClient::analyzeLog); history_ itself only ever keeps
    // the plain question, or every later turn would resend the whole excerpt.
    if (looksLikeLogQuestion(question.ToStdWstring())) {
        const std::wstring excerpt = readRecentLogEntries(maxLogExcerptChars);
        runAsync([client = client_, snapshot, excerpt]() { return client->analyzeLog(snapshot, excerpt); });
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
        // Only a real answer belongs in the history; an error message would be
        // sent back as if the assistant had said it.
        history_.push_back({ false, text.ToStdWstring() });
    }
    else
        appendTurn(_("Fehler"), text);
    input_->SetFocus();
}

void ChatDialog::onInputKey(wxKeyEvent& event)
{
    // Plain Enter inserts a line break in the multi-line field, so sending needs
    // a modifier.
    if (event.GetKeyCode() == WXK_RETURN && event.ControlDown() && sendButton_->IsEnabled()) {
        wxCommandEvent unused;
        onSend(unused);
        return;
    }
    event.Skip();
}

void ChatDialog::onClose(wxCloseEvent&)
{
    // Hidden rather than destroyed: the conversation stays available when the
    // user opens the chat again, and a request still running has a live window
    // to deliver its answer to.
    Hide();
}
