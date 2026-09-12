#include "pch.h"
#include "RecycleBin.h"
#include "FilesystemUtil.h"
#include "Logging.h"
#include "Util.h"
#include <system_error>

namespace AFS_Core {

namespace fs = std::filesystem;

namespace {
bool entry_exists(const fs::path& path)
{
    std::error_code ec;
    return fs::exists(fs::symlink_status(path, ec)) && !ec;
}

bool create_parent(const fs::path& path)
{
    if (path.parent_path().empty())
        return true;
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec)
        log_fatal(L"Papierkorb: Verzeichnis kann nicht angelegt werden", path.parent_path().wstring());
    return !ec;
}
}

RecycleBin::RecycleBin()
{
    const std::string home = fsu::home_dir();
    path_ = home.empty() ? fs::path(".recyclebin") : fs::path(home) / ".recyclebin";
}

RecycleBin::~RecycleBin() = default;

RecycleBin& RecycleBin::instance()
{
    static RecycleBin obj;
    return obj;
}

bool RecycleBin::check_relative(const fs::path& item) const  // [RecycleBin.md#check_relative]
{
    if (item.empty() || item.is_absolute() || item.has_root_name()) {
        log_error(L"Papierkorb: absoluter Pfad nicht erlaubt", item.wstring());
        return false;
    }
    for (const auto& part : item.lexically_normal())
        if (part == "..") {
            log_error(L"Papierkorb: Pfad fuehrt aus dem Papierkorb heraus", item.wstring());
            return false;
        }
    return true;
}

bool RecycleBin::move(const fs::path& from, const fs::path& to) const  // [RecycleBin.md#move]
{
    std::error_code ec;
    fs::rename(from, to, ec);
    if (!ec)
        return true;
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        log_error(L"Papierkorb: Verschieben fehlgeschlagen", from.wstring(), to.wstring(), util::string_to_wstring(ec.message()));
        return false;
    }
    fs::remove(from, ec);
    if (ec) {
        log_error(L"Papierkorb: Quelldatei konnte nicht geloescht werden", from.wstring());
        return false;
    }
    return true;
}

void RecycleBin::bin_path(const fs::path& path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = path;
}

fs::path RecycleBin::bin_path() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return path_;
}

bool RecycleBin::add_item(const fs::path& item)
{
    return add_item(item, item);
}

bool RecycleBin::add_item(const fs::path& source, const fs::path& item)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!check_relative(item))
        return false;
    if (!entry_exists(source)) {
        log_error(L"Papierkorb: Datei existiert nicht", source.wstring());
        return false;
    }
    const fs::path target = path_ / item.lexically_normal();
    if (!create_parent(target) || !move(source, target))
        return false;
    log_debug(L"Papierkorb: verschoben " + source.wstring());
    return true;
}

bool RecycleBin::restore_item(const fs::path& item)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!check_relative(item))
        return false;
    const fs::path target = item.lexically_normal();
    const fs::path source = path_ / target;
    if (!entry_exists(source)) {
        log_error(L"Papierkorb: Datei liegt nicht im Papierkorb", item.wstring());
        return false;
    }
    if (entry_exists(target)) {
        log_error(L"Papierkorb: Ziel existiert bereits", target.wstring());
        return false;
    }
    if (!create_parent(target) || !move(source, target))
        return false;
    log_debug(L"Papierkorb: wiederhergestellt " + target.wstring());
    return true;
}

RecycleBin::ItemList RecycleBin::list_items() const  // [RecycleBin.md#list_items]
{
    std::lock_guard<std::mutex> lock(mutex_);
    ItemList items;
    std::error_code ec;
    if (!fs::exists(path_, ec))
        return items;
    for (auto it = fs::recursive_directory_iterator(path_, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            log_error(L"Papierkorb: Verzeichnis nicht lesbar", path_.wstring());
            break;
        }
        if (!it->is_directory(ec))
            items.push_back(it->path().lexically_relative(path_));
    }
    return items;
}

} // namespace AFS_Core
