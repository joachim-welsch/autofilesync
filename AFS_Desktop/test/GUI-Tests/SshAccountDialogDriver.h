#pragma once

#include "ModalDialogWatcher.h"
#include <wx/string.h>

namespace guitest {

struct SshAccountInput {
    wxString name;
    wxString host;
    wxString user;
    wxString password;
};

// Fills the "SSH-Verbindung hinzufügen" dialog the way a user would and confirms
// it with OK. Pass the result to ModalDialogWatcher::expect().
DialogAction fillSshAccountDialog(const SshAccountInput& input);

// Presses "Hinzufügen" in the "SSH-Verbindungen" dialog and closes it with OK.
// The add dialog opens nested inside this one, so its own expectation has to be
// announced right after this one.
DialogAction addSshAccountAndClose();

}
