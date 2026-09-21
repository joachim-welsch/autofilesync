#pragma once

#include <wx/dialog.h>
#include <wx/modalhook.h>
#include <deque>
#include <functional>
#include <vector>

namespace guitest {

using DialogAction = std::function<void(wxDialog&)>;

class ModalDialogWatcher: public wxModalDialogHook {  // [ModalDialogWatcher.md#ModalDialogWatcher]
public:
    static ModalDialogWatcher& instance();
    void expect(const wxString& title, DialogAction action);
    void answer(const wxString& title, int buttonId);  // [ModalDialogWatcher.md#answer]
    bool hasPendingExpectations() const { return !expected_.empty(); }
    const std::vector<wxString>& unexpectedDialogs() const { return unexpected_; }
    const std::vector<wxString>& answeredMessages() const { return answered_; }
    void reset();
protected:
    int Enter(wxDialog* dialog) override;
private:
    struct Expectation { wxString title; DialogAction action; int answer = wxID_NONE; };
    std::deque<Expectation> expected_;
    std::vector<wxString> unexpected_;
    std::vector<wxString> answered_;
};

}
