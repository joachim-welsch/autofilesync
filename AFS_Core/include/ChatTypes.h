#pragma once

#include <string>
#include <vector>

namespace AFS_Core {

// One turn of the conversation, in the order a chat API expects them. Requests
// are stateless, so the whole history travels with every request.
struct ChatTurn {
    bool         fromUser = true;   // false: an earlier answer of the assistant
    std::wstring text;
};

using ChatHistory = std::vector<ChatTurn>;

struct ChatReply {
    bool         ok = false;
    std::wstring text;              // the answer, or why there is none
};

} // namespace AFS_Core
