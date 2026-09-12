#pragma once
#include <cstddef>
#include <string>

namespace AFS_Core {

std::wstring read_recent_log_entries(std::size_t max_chars);  // [LogAnalysis.md#read_recent_log_entries]

bool looks_like_log_question(const std::wstring& text);  // [LogAnalysis.md#looks_like_log_question]

} // namespace AFS_Core
