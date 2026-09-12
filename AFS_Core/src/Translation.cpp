#include "pch.h"
#include "Translation.h"
#include "Util.h"
#ifdef AFS_WITH_GETTEXT
    #include <libintl.h>
#endif

namespace AFS_Core {

namespace {
constexpr const char* TEXT_DOMAIN = "autofilesync";
}

namespace translation {
#ifdef AFS_WITH_GETTEXT
void init(const std::filesystem::path& locale_dir)
{
    if (locale_dir.empty())
        return;
    bindtextdomain(TEXT_DOMAIN, locale_dir.string().c_str());
    bind_textdomain_codeset(TEXT_DOMAIN, "UTF-8");  // [Translation.md#catalogue-codeset]
    textdomain(TEXT_DOMAIN);
}

std::wstring translate(const char* msgid)
{
    return util::string_to_wstring(dgettext(TEXT_DOMAIN, msgid));  // [Translation.md#dgettext-instead-of-gettext]
}
#else
void init(const std::filesystem::path&) {}  // [Translation.md#no-libintl-fallback]

std::wstring translate(const char* msgid)
{
    return util::string_to_wstring(msgid);
}
#endif
}

} // namespace AFS_Core
