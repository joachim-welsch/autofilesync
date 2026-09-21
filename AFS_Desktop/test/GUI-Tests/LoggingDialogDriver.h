#pragma once

#include "ModalDialogWatcher.h"
#include <wx/string.h>
#include <optional>

namespace guitest {

struct EmailInput {
    std::optional<bool> active;
    std::optional<wxString> address;
    std::optional<wxString> server;
    std::optional<int> port;
    std::optional<wxString> user;
    std::optional<wxString> password;
    std::optional<wxString> sender;
};

struct LoggingInput {  // [LoggingDialogDriver.md#LoggingInput]
    std::optional<wxString> logPath;
    std::optional<wxString> logLevel;
    std::optional<int> retentionDays;
    std::optional<wxString> editor;
    std::optional<wxString> backupProofPath;
    EmailInput email;
};

DialogAction fillLoggingDialog(const LoggingInput& input);  // [LoggingDialogDriver.md#fillLoggingDialog]

}
