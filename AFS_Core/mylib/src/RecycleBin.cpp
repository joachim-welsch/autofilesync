#include "pch.h"
#include "RecycleBin.h"
#include "FilesystemUtil.h"
#include "Logging.h"
#include "util.h"
#include <system_error>

namespace fs = std::filesystem;

namespace {
bool entryExists(const fs::path& path)
{
    std::error_code ec;
    return fs::exists(fs::symlink_status(path, ec)) && !ec;
}

bool createParent(const fs::path& path)
{
    if (path.parent_path().empty())
        return true;
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec)
        LOG_FATAL2(L"Papierkorb: Verzeichnis kann nicht angelegt werden", path.parent_path().wstring());
    return !ec;
}
}

RecycleBin::RecycleBin()
{
    const std::string home = fsu::homeDir();
    path_ = home.empty() ? fs::path(".recyclebin") : fs::path(home) / ".recyclebin";
}

RecycleBin::~RecycleBin() = default;

RecycleBin& RecycleBin::instance()
{
    static RecycleBin obj;
    return obj;
}

// An absolute path or a ".." component would place the item outside path_,
// which the bin must never touch.
bool RecycleBin::checkRelative(const fs::path& item) const
{
    if (item.empty() || item.is_absolute() || item.has_root_name()) {
        LOG_ERROR2(L"Papierkorb: absoluter Pfad nicht erlaubt", item.wstring());
        return false;
    }
    for (const auto& part : item.lexically_normal())
        if (part == "..") {
            LOG_ERROR2(L"Papierkorb: Pfad fuehrt aus dem Papierkorb heraus", item.wstring());
            return false;
        }
    return true;
}

// rename() fails across filesystem boundaries (EXDEV), which happens as soon as
// the bin lives on another drive than the file.
bool RecycleBin::move(const fs::path& from, const fs::path& to) const
{
    std::error_code ec;
    fs::rename(from, to, ec);
    if (!ec)
        return true;
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        LOG_ERROR4(L"Papierkorb: Verschieben fehlgeschlagen", from.wstring(), to.wstring(), util::string_to_wstring(ec.message()));
        return false;
    }
    fs::remove(from, ec);
    if (ec) {
        LOG_ERROR2(L"Papierkorb: Quelldatei konnte nicht geloescht werden", from.wstring());
        return false;
    }
    return true;
}

void RecycleBin::binPath(const fs::path& path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = path;
}

fs::path RecycleBin::binPath() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return path_;
}

bool RecycleBin::addItem(const fs::path& item)
{
    return addItem(item, item);
}

bool RecycleBin::addItem(const fs::path& source, const fs::path& item)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!checkRelative(item))
        return false;
    if (!entryExists(source)) {
        LOG_ERROR2(L"Papierkorb: Datei existiert nicht", source.wstring());
        return false;
    }
    const fs::path target = path_ / item.lexically_normal();
    if (!createParent(target) || !move(source, target))
        return false;
    LOG_DEBUG(L"Papierkorb: verschoben " + source.wstring());
    return true;
}

bool RecycleBin::restoreItem(const fs::path& item)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!checkRelative(item))
        return false;
    const fs::path target = item.lexically_normal();
    const fs::path source = path_ / target;
    if (!entryExists(source)) {
        LOG_ERROR2(L"Papierkorb: Datei liegt nicht im Papierkorb", item.wstring());
        return false;
    }
    if (entryExists(target)) {
        LOG_ERROR2(L"Papierkorb: Ziel existiert bereits", target.wstring());
        return false;
    }
    if (!createParent(target) || !move(source, target))
        return false;
    LOG_DEBUG(L"Papierkorb: wiederhergestellt " + target.wstring());
    return true;
}

// Reads the directory instead of a member list, so items survive a restart.
RecycleBin::ItemList RecycleBin::listItems() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    ItemList items;
    std::error_code ec;
    if (!fs::exists(path_, ec))
        return items;
    for (auto it = fs::recursive_directory_iterator(path_, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            LOG_ERROR2(L"Papierkorb: Verzeichnis nicht lesbar", path_.wstring());
            break;
        }
        if (!it->is_directory())
            items.push_back(it->path().lexically_relative(path_));
    }
    return items;
}
