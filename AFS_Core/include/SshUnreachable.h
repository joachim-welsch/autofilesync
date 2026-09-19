#pragma once
#include <stdexcept>

namespace AFS_Core {

struct SshUnreachable : std::runtime_error {  // [SftpConnection.md#SshUnreachable]
    using std::runtime_error::runtime_error;
};

} // namespace AFS_Core
