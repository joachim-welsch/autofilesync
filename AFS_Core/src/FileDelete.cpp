#include "pch.h"
#include "FileDelete.h"
#include "Config.h"
#include <RecycleBin.h>
#include <Util.h>
#include <Logging.h>
#include <filesystem>

namespace AFS_Core {

namespace fs = std::filesystem;

bool delete_file(const std::wstring& pathname)
{
    if (Config::instance().settings().delete_mode() == DeleteMode::Permanent)
        return util::remove_file(pathname, LogLevel::Error);
    const fs::path source(pathname);
    const fs::path item = util::with_timestamp_prefix(source.relative_path());  // [FileDelete.md#recycle-bin-layout]
    if (!RecycleBin::instance().add_item(source, item)) {
        log_error(L"Verschieben in den Papierkorb fehlgeschlagen: ", pathname);
        return false;
    }
    log_info(L"In den Papierkorb verschoben: ", pathname);
    return true;
}

} // namespace AFS_Core
