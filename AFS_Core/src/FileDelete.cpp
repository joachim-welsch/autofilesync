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
    // relative_path() strips the root ("C:\" resp. "/") so the origin's
    // directory structure is rebuilt below the bin; the timestamp prefix keeps
    // files deleted repeatedly from the same place apart.
    const fs::path item = util::with_timestamp_prefix(source.relative_path());
    if (!RecycleBin::instance().add_item(source, item)) {
        log_error(L"Verschieben in den Papierkorb fehlgeschlagen: ", pathname);
        return false;
    }
    log_info(L"In den Papierkorb verschoben: ", pathname);
    return true;
}

} // namespace AFS_Core
