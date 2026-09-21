#include "ModalDialogWatcher.h"
#include <wx/msgdlg.h>

namespace guitest {

ModalDialogWatcher& ModalDialogWatcher::instance()
{
    static ModalDialogWatcher* watcher = [] {  // [ModalDialogWatcher.md#never-destroyed]
        auto* created = new ModalDialogWatcher;
        created->Register();
        return created;
    }();
    return *watcher;
}

void ModalDialogWatcher::expect(const wxString& title, DialogAction action)
{
    expected_.push_back({title, std::move(action)});
}

void ModalDialogWatcher::answer(const wxString& title, int buttonId)
{
    expected_.push_back({title, DialogAction(), buttonId});
}

void ModalDialogWatcher::reset()
{
    expected_.clear();
    unexpected_.clear();
    answered_.clear();
}

int ModalDialogWatcher::Enter(wxDialog* dialog)
{
    if (expected_.empty() || expected_.front().title != dialog->GetTitle()) {
        unexpected_.push_back(dialog->GetTitle());
        return wxID_CANCEL;
    }
    const Expectation expectation = expected_.front();
    expected_.pop_front();
    if (expectation.answer != wxID_NONE) {  // [ModalDialogWatcher.md#answer]
        auto* messageDialog = dynamic_cast<wxMessageDialog*>(dialog);
        answered_.push_back(messageDialog != nullptr ? messageDialog->GetMessage() : dialog->GetTitle());
        return expectation.answer;
    }
    dialog->CallAfter([dialog, action = expectation.action] { action(*dialog); });  // [ModalDialogWatcher.md#queued-not-called]
    return wxID_NONE;
}

}
