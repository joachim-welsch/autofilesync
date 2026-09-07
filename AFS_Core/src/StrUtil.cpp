#include "pch.h"
#include <StrUtil.h>
#include <codecvt>
#include <locale>
#ifdef WIN32
   #include <windows.h>
#endif

namespace AFS_Core {

using std::string;
using std::wstring;

namespace util {

#ifdef WIN32
std::string wstring_to_string(const std::wstring& ws) {
    if (ws.empty()) {
        return {};
    }
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bufferSize == 0) {
        throw std::runtime_error("WideCharToMultiByte failed");
    }
    std::string result(bufferSize - 1, '\0'); // -1, because WideCharToMultiByte counts the null terminator
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &result[0], bufferSize, nullptr, nullptr);
    return result;
}
std::wstring string_to_wstring(const std::string& s)
{
     if (s.empty())
        return {};
    const int size = static_cast<int>(s.size());
    UINT codepage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    int need = MultiByteToWideChar(codepage, flags, s.c_str(), size, nullptr, 0);
    if (need == 0) {
         codepage = CP_ACP;
        flags = 0;
        need = MultiByteToWideChar(codepage, flags, s.c_str(), size, nullptr, 0);
        if (need == 0)
            return {};
    }
    std::wstring result(static_cast<size_t>(need), L'\0');
    MultiByteToWideChar(codepage, flags, s.c_str(), size, &result[0], need);
    return result;
}
#else
std::string wstring_to_string(const std::wstring& wstr)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;
    return converterX.to_bytes(wstr);
}
std::wstring string_to_wstring(const std::string& str)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;
    return converterX.from_bytes(str);
}

#endif

}

} // namespace AFS_Core
