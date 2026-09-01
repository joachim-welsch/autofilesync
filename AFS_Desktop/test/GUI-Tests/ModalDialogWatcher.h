#pragma once

#include <wx/dialog.h>
#include <wx/modalhook.h>
#include <deque>
#include <functional>
#include <vector>

namespace guitest {

using DialogAction = std::function<void(wxDialog&)>;

// The single modal hook of the test run. A dialog a test announced through
// expect() is driven from inside its own modal loop; every other modal dialog
// is cancelled and recorded, so an unforeseen popup fails the test instead of
// blocking it forever.
class ModalDialogWatcher: public wxModalDialogHook {
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
