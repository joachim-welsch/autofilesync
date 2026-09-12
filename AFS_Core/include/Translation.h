#pragma once
#include <filesystem>
#include <initializer_list>
#include <string>

namespace AFS_Core {

namespace translation {  // [Translation.md#translation]
void init_locale(const char* fallback);  // [Translation.md#init_locale]
std::filesystem::path locale_dir();  // [Translation.md#locale_dir]
void init(const std::filesystem::path& locale_dir);  // [Translation.md#init]
constexpr const char* msgid(const char* id) { return id; }  // [Translation.md#msgid]
std::wstring translate(const char* msgid);  // translation of msgid for the language taken from LANGUAGE/LC_MESSAGES
std::string translate_utf8(const char* msgid);  // [Translation.md#translate_utf8]
std::wstring translate(const char* msgid, std::initializer_list<std::wstring> values);  // [Translation.md#placeholders]
std::string translate_utf8(const char* msgid, std::initializer_list<std::string> values);
}

} // namespace AFS_Core
