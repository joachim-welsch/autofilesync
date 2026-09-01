#include "pch.h"
#include <strutil.h>
#include <codecvt>
#include <locale>
#ifdef WIN32
   #include <windows.h>
#endif

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
    // Muss die Umkehrung von wstring_to_string() sein: das kodiert nach UTF-8
    // (CP_UTF8), also muss hier UTF-8 dekodiert werden. Die frueher benutzte
    // byteweise Verbreiterung machte aus jedem Mehrbyte-Zeichen mehrere
    // Fantasiezeichen - aus "ue" wurde "A-Tilde plus Viertel-Zeichen" -, womit
    // jeder Rundlauf mit Umlauten (Pfade, SSH-Zugangsdaten, Fehlertexte)
    // stillschweigend Datenmuell erzeugte.
    if (s.empty())
        return {};
    const int size = static_cast<int>(s.size());
    UINT codepage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    int need = MultiByteToWideChar(codepage, flags, s.c_str(), size, nullptr, 0);
    if (need == 0) {
        // Kein gueltiges UTF-8: Texte aus Windows-APIs und aus what() von
        // std::filesystem_error kommen in der ANSI-Codepage. Die zu
        // interpretieren ist allemal besser, als Ersatzzeichen zu liefern.
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
    // Must be the inverse of string_to_wstring() below; a naive per-character
    // cast truncates any non-ASCII code point instead of UTF-8-encoding it.
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
