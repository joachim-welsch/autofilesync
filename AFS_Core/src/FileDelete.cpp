#include "pch.h"
#include "FileDelete.h"
#include "Config.h"
#include <RecycleBin.h>
#include <util.h>
#include <Logging.h>
#include <filesystem>

namespace AFS_Core {

namespace fs = std::filesystem;

bool deleteFile(const std::wstring& pathname)
{
    if (Config::instance().settings().deleteMode() == DeleteMode::Permanent)
        return util::remove_file(pathname, LogLevel::Error);
    const fs::path source(pathname);
    // relative_path() strips the root ("C:\" resp. "/") so the origin's
    // directory structure is rebuilt below the bin; the timestamp prefix keeps
    // files deleted repeatedly from the same place apart.
    const fs::path item = util::withTimestampPrefix(source.relative_path());
    if (!RecycleBin::instance().addItem(source, item)) {
        LOG_ERROR2(L"Verschieben in den Papierkorb fehlgeschlagen: ", pathname);
        return false;
    }
    LOG_INFO2(L"In den Papierkorb verschoben: ", pathname);
    return true;
}

} // namespace AFS_Core
