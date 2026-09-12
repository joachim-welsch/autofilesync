#include "pch.h"
#include "FileDelete.h"
#include "Config.h"
#include <RecycleBin.h>
#include <Util.h>
#include <Logging.h>
#include <Translation.h>
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
        log_error(translation::translate("file_move_to_recycle_bin_failed"), pathname);
        return false;
    }
    log_info(translation::translate("file_moved_to_recycle_bin"), pathname);
    return true;
}

} // namespace AFS_Core
