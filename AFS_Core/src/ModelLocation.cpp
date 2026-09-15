#include "pch.h"
#include "ModelLocation.h"
#include <cstdlib>
#include <optional>
#include <string>
#include <system_error>

namespace AFS_Core {

namespace model_location {

namespace fs = std::filesystem;

namespace {
constexpr char MODEL_EXTENSION[] = ".gguf";

std::optional<fs::path> env_path(const char* name)
{
#ifdef _WIN32
    const std::wstring wide_name(name, name + std::char_traits<char>::length(name));
    const wchar_t* value = _wgetenv(wide_name.c_str());  // [ModelLocation.md#wide-environment]
#else
    const char* value = std::getenv(name);
#endif
    if (value == nullptr || *value == 0)
        return std::nullopt;
    return fs::path(value);
}

fs::path data_dir()
{
#ifdef _WIN32
    if (const auto local = env_path("LOCALAPPDATA"))  // [ModelLocation.md#local-not-roaming]
        return *local;
    const auto profile = env_path("USERPROFILE");
    return profile ? *profile / "AppData" / "Local" : fs::path();
#elif defined(_MACOS)
    const auto home = env_path("HOME");
    return home ? *home / "Library" / "Application Support" : fs::path();
#else
    if (const auto xdg = env_path("XDG_DATA_HOME"))
        return *xdg;
    const auto home = env_path("HOME");
    return home ? *home / ".local" / "share" : fs::path();
#endif
}

bool is_model_file(const fs::directory_entry& entry)
{
    std::error_code ec;
    return entry.path().extension() == MODEL_EXTENSION && entry.is_regular_file(ec);  // [ModelLocation.md#partial-downloads]
}
}

fs::path directory()
{
    const fs::path base = data_dir();
    return base.empty() ? base : base / "autofilesync" / "models";
}

fs::path find()
{
    if (const auto configured = env_path(PATH_VARIABLE))
        return *configured;
    std::error_code ec;
    fs::path first;
    for (fs::directory_iterator it(directory(), ec), end; !ec && it != end; it.increment(ec))
        if (is_model_file(*it) && (first.empty() || it->path().filename() < first.filename()))  // [ModelLocation.md#first-by-name]
            first = it->path();
    return first;
}

}

} // namespace AFS_Core
