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
    // The catalogues are UTF-8 and util::string_to_wstring() decodes UTF-8;
    // without this gettext would recode to the charset of the current locale.
    bind_textdomain_codeset(TEXT_DOMAIN, "UTF-8");
    textdomain(TEXT_DOMAIN);
}

std::wstring translate(const char* msgid)
{
    // dgettext() instead of gettext(): the core must not depend on whichever
    // domain the hosting program set last.
    return util::string_to_wstring(dgettext(TEXT_DOMAIN, msgid));
}
#else
// Builds without libintl (Windows without a gettext package) keep the German
// message ids - the same texts the untranslated program used.
void init(const std::filesystem::path&) {}

std::wstring translate(const char* msgid)
{
    return util::string_to_wstring(msgid);
}
#endif
}

} // namespace AFS_Core
