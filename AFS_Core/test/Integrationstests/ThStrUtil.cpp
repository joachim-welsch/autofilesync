#include "pch.h"
#include "ThStrUtil.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace tu {
    std::string toUtf8(const std::wstring& in)
    {
#ifdef _WIN32
        if (in.empty()) return {};
        const int n = ::WideCharToMultiByte(CP_UTF8, 0, in.data(), static_cast<int>(in.size()),
            nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(n), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, in.data(), static_cast<int>(in.size()),
            out.data(), n, nullptr, nullptr);
        return out;
#else
        return std::filesystem::path(in).string();   // wchar_t(32bit) -> UTF-8
#endif
    }
}