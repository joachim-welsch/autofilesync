#include "pch.h"
#include "Translation.h"
#include "util.h"
#ifdef AFS_WITH_GETTEXT
    #include <libintl.h>
#endif

namespace {
constexpr const char* textDomain = "autofilesync";
}

namespace translation {
#ifdef AFS_WITH_GETTEXT
void init(const std::filesystem::path& localeDir)
{
    if (localeDir.empty())
        return;
    bindtextdomain(textDomain, localeDir.string().c_str());
    // The catalogues are UTF-8 and util::string_to_wstring() decodes UTF-8;
    // without this gettext would recode to the charset of the current locale.
    bind_textdomain_codeset(textDomain, "UTF-8");
    textdomain(textDomain);
}

std::wstring translate(const char* msgid)
{
    // dgettext() instead of gettext(): the core must not depend on whichever
    // domain the hosting program set last.
    return util::string_to_wstring(dgettext(textDomain, msgid));
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
