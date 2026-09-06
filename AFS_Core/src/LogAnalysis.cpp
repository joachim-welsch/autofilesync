#include "pch.h"
#include "LogAnalysis.h"
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <Logging.h>
#include "util.h"

namespace AFS_Core {

using util::string_to_wstring;
using util::wstring_to_string;

std::wstring readRecentLogEntries(std::size_t maxChars)
{
    const std::wstring path = Logging::instance().getLogFilePathName();
    std::ifstream is(wstring_to_string(path), std::ios::in | std::ios::binary);
    if (!is.is_open())
        return {};
    is.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(is.tellg());
    const std::size_t start = size > maxChars ? size - maxChars : 0;
    is.seekg(static_cast<std::streamoff>(start));
    std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    // Drop the partial line seekg() landed in the middle of, so the excerpt
    // starts at a real log entry instead of a truncated one.
    if (start > 0) {
        const auto firstNewline = content.find('\n');
        content.erase(0, firstNewline == std::string::npos ? content.size() : firstNewline + 1);
    }
    return string_to_wstring(content);
}

bool looksLikeLogQuestion(const std::wstring& text)
{
    std::wstring lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return lower.find(L"log") != std::wstring::npos || lower.find(L"protokoll") != std::wstring::npos;
}

} // namespace AFS_Core
