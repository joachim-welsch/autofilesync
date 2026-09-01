#include "config.h"
#include "util.h"
#include <fstream>
#include <string>
#include <iostream>
#ifdef _WXWIDGETS
    #include "wx/msgdlg.h"
#endif
#include <stdlib.h>
#include "Logging.h"
#include "winutil.h"

using std::wstring;
using std::string;
using util::toString;
using util::string_to_wstring;
using std::make_pair;
namespace pt = boost::property_tree;

Config* Config::instance_ = nullptr;
std::once_flag Config::initInstanceFlag_;

SyncFlags::SyncFlags(bool delAftercopy, bool tws, bool cwtn, bool fsp, bool enabled)
    : delAftercopy_(delAftercopy)
    , twoWaySync_(tws)
    , copyWhenTargetNewer_(cwtn)
    , fatherSonPrinciple_(fsp)
    , enabled_(enabled)
{}

#ifdef _UNIX
string Config::getRootPathname()
{
    return "/home/welschj/backgroundsync.cfg";
}

#else
wstring Config::getRootPathname()
{
    wchar_t* upstr;
    size_t* sz = nullptr;
    _wdupenv_s(&upstr, sz, L"USERPROFILE");
    wstring retval;
    if (upstr != nullptr) {
        retval = wstring(upstr) + L"\\autofilesync.json";
        free(upstr);
    }
    return retval;
}
#endif

void Config::reset()
{
    settings_.reset();
    syncitems_.clear();
}

wstring getline(std::wfstream& in)
{
    wstring retval;
    do {
        std::getline(in, retval);
        if (retval.length() > 1 && retval.substr(0, 2) == L"//")
            retval = L"";  // remove comment
        if (!in)
            break;
    } while (retval.empty());
    return retval;
}

CompareItems getCompareItems(boost::property_tree::wptree& ci)
{
    CompareItems retval;
    for (const auto& i: ci) {
        const wstring x=i.second.data();
        retval.insert(str2CompareItem(x));
    }
    return retval;
}

Exclusions getExclusions(boost::property_tree::wptree& /*ci*/)
{
    Exclusions retval; // TODO
    //for (const auto& i : ci) {
    //    const string x = i.second.data();
    //    retval.insert(str2CompareItem(x));
    //}
    return retval;
}

SyncFlags getSyncFlags(boost::property_tree::wptree& sf)
{
    SyncFlags retval;
    retval.delAftercopy(sf.get<wstring>(L"delAftercopy") == L"true");
    retval.twoWaySync(sf.get<wstring>(L"twoWaySync") == L"true");
    retval.copyWhenTargetNewer(sf.get<wstring>(L"copyWhenTargetNewer") == L"true");
    retval.fatherSonPrinciple(sf.get<wstring>(L"fatherSonPrinciple") == L"true");
    retval.enabled(sf.get<wstring>(L"enabled") == L"true");
    return retval;
}

Settings getSettings(boost::property_tree::wptree& sf)
{
    Settings retval;
    retval.startMinimized(sf.get<wstring>(L"StartMinimzed") == L"true");
    retval.minimizeInsteadOfClose(sf.get<wstring>(L"MinimizeInsteadOfClose") == L"true");
    retval.autoStart(sf.get<wstring>(L"Autostart") == L"true");
    retval.startSyncAtStart(sf.get<wstring>(L"StartSyncAtStart") == L"true");
    retval.showErrors(sf.get<wstring>(L"ShowErrors") == L"true");
    retval.showWarnings(sf.get<wstring>(L"ShowWarnings") == L"true");
    retval.showInfos(sf.get<wstring>(L"ShowInfos") == L"true");
    return retval;
}

void Config::writeSettings(boost::property_tree::wptree& root)
{
    pt::wptree settings;
    settings.put(wstring(L"StartMinimzed"), toString(settings_.startMinimized()));
    settings.put(wstring(L"MinimizeInsteadOfClose"), toString(settings_.minimizeInsteadOfClose()));
    settings.put(wstring(L"Autostart"), toString(settings_.autoStart()));
    settings.put(wstring(L"StartSyncAtStart"), toString(settings_.startSyncAtStart()));
    settings.put(wstring(L"ShowErrors"), toString(settings_.showErrors()));
    settings.put(wstring(L"ShowWarnings"), toString(settings_.showWarnings()));
    settings.put(wstring(L"ShowInfos"), toString(settings_.showInfos()));
    root.push_back(make_pair(wstring(L"Settings"), settings));
}


bool interpretSyncItem(boost::property_tree::wptree syncitemNode, SyncItem& si)
{
    try {
        //for (auto& it : syncitemNode) {
        //    wxMessageBox(it.first  + it.second.data());
        //}
        //auto ci = syncitemNode.get_child("compareitems");
        auto compareitems = syncitemNode.get_child_optional(L"compareitems");
        if (compareitems != boost::none) {
            si.compareItems(getCompareItems(compareitems.get()));
        }
        auto exclusions = syncitemNode.get_child_optional(L"exclusions");
        if (exclusions != boost::none) {
            si.exclusions(getExclusions(exclusions.get()));
        }
        auto syncflags = syncitemNode.get_child_optional(L"syncflags");
        if (syncflags != boost::none) {
            auto flags = getSyncFlags(syncflags.get());
            si.flags(flags);
        }
        wstring name = syncitemNode.get<wstring>(L"name");
        wstring type = syncitemNode.get<wstring>(L"type");
        wstring source = syncitemNode.get<wstring>(L"source");
        wstring target = syncitemNode.get<wstring>(L"target");
        wstring wildcard = syncitemNode.get<wstring>(L"wildcard");
        si.name(name);
        si.syncType(str2Type(type));
        si.source(source);
        si.target(target);
        si.wildcard(wildcard);
        si.syncCycle(std::stoi(syncitemNode.get<wstring>(L"SyncCycle")));
    } catch (std::exception& ex) {
        LOG_ERROR2("Lese Sync-Einstellungen: ", ex.what())
        return false;
    }
    return true;
}

bool Config::readLoggingSetup(boost::property_tree::wptree& root)
{
    try {
        auto logging = root.get_child_optional(L"Logging");
        if (logging != boost::none) {
            wstring LogLevel = logging->get<wstring>(L"LogLevel");
            Logging& log = Logging::instance();
            log.setLogLevel(log.getFromString(LogLevel));
            wstring LogFilePathName = logging->get<wstring>(L"LogFilePathName");
            log.setLogFilePathName(LogFilePathName);
            log.retentionDays(std::stoi(logging->get<wstring>(L"RetentionDays")));
            log.setEditorPathName(logging->get<wstring>(L"EditorPathName"));
        }
    }
    catch (std::exception& ex) {
        LOG_ERROR2("Lese Logging-Einstellungen: ", ex.what());
        return false;
    }
    return true;
}

bool Config::readSettings(boost::property_tree::wptree& root)
{
    try {
        auto settings = root.get_child_optional(L"Settings");
        if (settings != boost::none) {
            Settings s = getSettings(settings.value());
            settings_ = s;
        }
    }
    catch (std::exception& /*ex*/) {
        //wxMessageBox(wxString("read settings: ") + ex.what());
        return false;
    }
    return true;
}

bool Config::interpretJson(boost::property_tree::wptree&& root)
{
    if (root.empty())
        return false;
    if (!readSettings(root))
        return false;
    if (!readLoggingSetup(root))
        return false;
    auto syncitems = root.get_child_optional(L"Syncitems");
    if (syncitems == boost::none)
        return true;
    for (auto iter = syncitems.value().begin(); iter != syncitems.value().end(); iter++) {
        SyncItem si;
        interpretSyncItem(iter->second, si);
        add(si, true);
    }
    return true;
}

void Config::writeLoggingSetup(boost::property_tree::wptree& root)
{
	pt::wptree logging;
    Logging& log = Logging::instance();
	logging.put(wstring(L"LogLevel"), string_to_wstring(string(log.logLevelStr(log.getLogLevel()))));
	logging.put(wstring(L"LogFilePathName"), log.getLogFilePathName());
    logging.put(wstring(L"RetentionDays"), toString(log.retentionDays()));
    logging.put(wstring(L"EditorPathName"), log.getEditorPathName());
	root.push_back(make_pair(wstring(L"Logging"), logging));
}

void Config::writeSyncItem(const SyncItem& inode, boost::property_tree::wptree& syncitemNode)
{
    pt::wptree syncFlagsNode;
    syncFlagsNode.put(wstring(L"delAftercopy"), toString(inode.delAftercopy()));
    syncFlagsNode.put(wstring(L"twoWaySync"), toString(inode.twoWaySync()));
    syncFlagsNode.put(wstring(L"copyWhenTargetNewer"), toString(inode.copyWhenTargetNewer()));
    syncFlagsNode.put(wstring(L"fatherSonPrinciple"), toString(inode.fatherSonPrinciple()));
    syncFlagsNode.put(wstring(L"delAftercopy"), toString(inode.delAftercopy()));
    syncFlagsNode.put(wstring(L"enabled"), toString(inode.enabled()));
    pt::wptree compareItemsNode;
    for (const auto& ci : inode.compareItems()) {
        pt::wptree compareItemNode;
        compareItemNode.put(L"", compareItem2Str(ci));
        compareItemsNode.push_back(std::make_pair(L"", compareItemNode));
    }
    pt::wptree exclusionsNode;
    for (const auto& ci : inode.exclusions()) {
        pt::wptree exclusionItemNode;
        exclusionItemNode.put(L"", ci);
        exclusionsNode.push_back(std::make_pair(L"", exclusionItemNode));
    }
    syncitemNode.push_back(make_pair(wstring(L"compareitems"), compareItemsNode));
    syncitemNode.push_back(make_pair(wstring(L"exclusions"), exclusionsNode));
    syncitemNode.push_back(make_pair(wstring(L"syncflags"), syncFlagsNode));
    syncitemNode.put(wstring(L"name"), inode.name());
    syncitemNode.put(wstring(L"type"), inode.typeStr());
    syncitemNode.put(wstring(L"source"), inode.source());
    syncitemNode.put(wstring(L"target"), inode.target());
    syncitemNode.put(wstring(L"wildcard"), inode.wildcard());
    syncitemNode.put(wstring(L"SyncCycle"), toString(inode.syncCycle()));
}

void Config::writeSyncItems(boost::property_tree::wptree& root)
{
    pt::wptree syncitemsNode;
    for (const auto& inode : syncitems_)
    {
        pt::wptree syncitemNode;
        writeSyncItem(inode, syncitemNode);
        syncitemsNode.push_back(make_pair(wstring(L"Syncitem"), syncitemNode));
    }
    root.push_back(make_pair(wstring(L"Syncitems"), syncitemsNode));
}

bool Config::write(const std::wstring& pathname)
{
    pt::wptree root;
    writeSettings(root);
    writeLoggingSetup(root);
    writeSyncItems(root);
    std::wfstream os(pathname, std::ios_base::out);
    try {
        pt::write_json(os, root);
    }
    catch (std::exception& ex) {
        wxMessageBox(wxString("write_json: ") + ex.what());
        return false;
    }
    bool retval = os.good();  // TODO handle error
    if (changed_)
        changed_ = !retval;
    return retval;
}

bool Config::read(const std::wstring& pathname)
{
    pathname_ = getRootPathname();
    reset();
    ready_ = false;
    if (!interpretJson(util::parseJsonFile(pathname.c_str()))) {
        reset();
        wxMessageBox(wxString("Konfiguration konnte nicht gelesen werden."));
    }
    ready_ = true;
    changed_= false;
    return true;
}

void Config::add(const SyncItem& si, bool loading)
{
    syncitems_.push_back(si);
    if (!loading)
        changed_ = true;
}

void Config::set(unsigned i, const SyncItem& si)
{
    syncitems_[i]=si;
    changed_ = true;
}

void Config::remove(unsigned i)
{
    auto iter=syncitems_.begin();
    std::advance(iter, i);
    syncitems_.erase(iter);
    changed_ = true;
}

bool Config::erase(SyncItem* item)
{
    auto found = std::find(syncitems_.begin(), syncitems_.end(), *item);
    if (found == syncitems_.end())
        return false;
    syncitems_.erase(found);
    changed_ = true;
    return true;
}

void Config::pathname(const std::wstring& p)
{
    pathname_=p;
}

const SyncItem& Config::item(unsigned i) const
{
    return syncitems_[i];
}

std::optional<SyncItem*> Config::find(const std::wstring& path, bool source)
{
    for (SyncItem& i: syncitems_) {
        if ((source && i.source() == path) || (!source && i.target() == path))
            return std::optional<SyncItem*>(&i);
    }
    return std::nullopt;
}

std::optional<SyncItem*> Config::findByName(const std::wstring& name)
{
    for (SyncItem& i : syncitems_) {
        if (i.name() == name)
            return std::optional<SyncItem*>(&i);
    }
    return std::nullopt;
}
//########################################################################################################

