#include "ModalDialogWatcher.h"

namespace guitest {

ModalDialogWatcher& ModalDialogWatcher::instance()
{
    // Deliberately never destroyed: unregistering happens in the destructor and
    // would touch wxWidgets' static hook list, whose lifetime relative to a
    // static in this translation unit is not defined.
    static ModalDialogWatcher* watcher = [] {
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
    // Queued instead of called here: the modal event loop is not running yet
    // and the dialog is not on screen, but wxUIActionSimulator needs a mapped
    // window to send its events to.
    dialog->CallAfter([dialog, action] { action(*dialog); });
    return wxID_NONE;
}

}
