#pragma once
#include <filesystem>
#include <string>

namespace AFS_Core {

namespace translation {  // [Translation.md#translation]
void init(const std::filesystem::path& locale_dir);  // [Translation.md#init]
std::wstring translate(const char* msgid);  // translation of msgid for the language taken from LANGUAGE/LC_MESSAGES
}

} // namespace AFS_Core
