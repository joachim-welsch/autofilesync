#include "ModalDialogWatcher.h"

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

void ModalDialogWatcher::reset()
{
    expected_.clear();
    unexpected_.clear();
}

int ModalDialogWatcher::Enter(wxDialog* dialog)
{
    if (expected_.empty() || expected_.front().title != dialog->GetTitle()) {
        unexpected_.push_back(dialog->GetTitle());
        return wxID_CANCEL;
    }
    const DialogAction action = expected_.front().action;
    expected_.pop_front();
    dialog->CallAfter([dialog, action] { action(*dialog); });  // [ModalDialogWatcher.md#queued-not-called]
    return wxID_NONE;
}

}
