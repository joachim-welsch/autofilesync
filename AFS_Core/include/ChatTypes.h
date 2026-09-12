#pragma once

#include <string>
#include <vector>

namespace AFS_Core {

struct ChatTurn {  // [ChatTypes.md#chat-turn]
    bool         from_user = true;   // false: an earlier answer of the assistant
    std::wstring text;
};

using ChatHistory = std::vector<ChatTurn>;

struct ChatReply {
    bool         ok = false;
    std::wstring text;              // the answer, or why there is none
};

} // namespace AFS_Core
