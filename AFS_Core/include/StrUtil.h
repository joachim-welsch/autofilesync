#pragma once
#include <string>

namespace AFS_Core {

namespace util {

    std::string wstring_to_string(const std::wstring& ws);
    std::wstring string_to_wstring(const std::string& s);
}

} // namespace AFS_Core
