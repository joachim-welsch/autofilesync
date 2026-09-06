#pragma once
#include <filesystem>
#include <string>

namespace AFS_Core {

// GNU gettext binding of the sync core. The message ids are the German source
// strings, so a missing or unreadable catalogue falls back to exactly the text
// the program showed before it was translated.
namespace translation {
// Binds the catalogue "autofilesync" below localeDir, where gettext expects
// <localeDir>/<language>/LC_MESSAGES/autofilesync.mo. An empty path leaves the
// system search path in place - callers cannot always locate the directory.
void init(const std::filesystem::path& localeDir);
// Translation of msgid for the language taken from LANGUAGE/LC_MESSAGES.
std::wstring translate(const char* msgid);
}

} // namespace AFS_Core
