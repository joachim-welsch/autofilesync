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
    bool hasPendingExpectations() const { return !expected_.empty(); }
    const std::vector<wxString>& unexpectedDialogs() const { return unexpected_; }
    void reset();
protected:
    int Enter(wxDialog* dialog) override;
private:
    struct Expectation { wxString title; DialogAction action; };
    std::deque<Expectation> expected_;
    std::vector<wxString> unexpected_;
};

}
