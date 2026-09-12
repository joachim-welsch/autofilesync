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

DialogAction fillSshAccountDialog(const SshAccountInput& input);  // [SshAccountDialogDriver.md#fillSshAccountDialog]

DialogAction addSshAccountAndClose();  // [SshAccountDialogDriver.md#addSshAccountAndClose]

}
