#pragma once

#include <string>
#include <vector>
#ifdef _WXWIDGETS
    #include <wx/app.h>
#endif

namespace AFS_Core {

using StringVector = std::vector<std::wstring>;
using WStringVector = std::vector<std::wstring>;

enum class SyncType {
    None,
    CountFiles,
    Single,
    Continuous
};

using StrVect = std::vector<std::wstring>;

#ifdef WIN32
    constexpr wchar_t DIR_SEPARATOR = '\\';
#else
    constexpr wchar_t DIR_SEPARATOR = L'/';
#endif

#ifdef _WXWIDGETS
    class Global {
        wxApp* app_;
        StrVect args_;
        Global() = default;
        Global(const Global&) = delete;
        Global& operator=(const Global&) = delete;
        Global(Global&&) = delete;
        Global& operator=(Global&&) = delete;
        ~Global() = default;
    public:
        static Global& instance();
        void set_app(wxApp* app) { app_ = app; }
        wxApp* get_app() const { return app_; }
    };
#endif

} // namespace AFS_Core
