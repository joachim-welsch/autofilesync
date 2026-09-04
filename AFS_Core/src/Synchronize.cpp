#include "pch.h"
#include "Synchronize.h"
#ifdef _WXWIDGETS
    #ifdef WIN32
        #include <Windows.h>
        #include <tchar.h>
        #include <wincon.h>
    #endif
#endif
#include <string>
#include <iostream>
#include <fstream>
#include "util.h"
#include "Config.h"
#include <sstream>
#include <filesystem>
#include "Global.h"
#include "SyncItem.h"
#include "SyncFileSsh.h"
#include "SyncFile.h"
#include "SyncTree.h"
#include "SyncDir.h"
#include "SyncControler.h"
#include "MountedDrives.h"
#include "BackupProof.h"
#include <FilesystemUtil.h>
#include <utility>


using namespace util;
namespace fs = std::filesystem;
using std::wstring;
using std::ostringstream;
using std::wstring;
using util::withTrailingChar;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class Synchronize
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::atomic<Synchronize*> Synchronize::this_ = nullptr;

std::atomic_bool Synchronize::stop_ = ATOMIC_VAR_INIT(true);
std::atomic<SyncType> syncType_ = ATOMIC_VAR_INIT(SyncType::Continuous);

bool Synchronize::hasInstance()
{
    return this_.load() != nullptr;
}

bool Synchronize::stopRequested()
{
    return stop_;
}

Synchronize& Synchronize::instance()
{
    LOG_DEBUG(L"Synchronize::instance()");
    if (this_.load() != nullptr)
        return  *this_.load();
	else 
        throw std::runtime_error("Keine Synchronisation aktiv.");
}

Synchronize::Synchronize(SyncType syncType, SyncControler* sc, SyncRole role)
  : syncType_(syncType),
    syncControler_(sc),
    role_(role),
    arbiter_(role)
{
    // stop_ must be false before the thread starts, or a thread that reaches
    // its "while (... && !stop_)" check before create() runs the line below
    // sees the stale "true" from program start and exits after one pass.
    stop_ = false;
    thread_ = std::thread([this]() { (*this)(); });
}

void Synchronize::destroy()
{
    LOG_DEBUG(L"Synchronize::destroy()");
    Synchronize* inst = this_.load();
    if (inst != nullptr) {
        delete inst;
        this_.store(nullptr);
    }
}

Synchronize* Synchronize::create(SyncType syncType, SyncControler* sc, SyncRole role)
{
    LOG_DEBUG(L"Synchronize::create()");
    // stop_ is already false by the time the constructor returns; see there.
    Synchronize* retval = new Synchronize(syncType, sc, role);
    this_.store(retval);
    return retval;
}

bool Synchronize::hasControler() const 
{
    return syncControler_ != nullptr;
}

Synchronize::~Synchronize()
{
    LOG_DEBUG(L"Synchronize::~Synchronize()");
    stopRequest_ = true;
    stop_ = true;
    cv_.notify_all();
    if (thread_.joinable())
        thread_.join();
    this_.store(nullptr);
    LOG_DEBUG(L"Synchronize::~Synchronize() complete");
}

#ifdef _TEST
#include <mutex>
std::once_flag removeTestFlag;
void removeTestFile() {
    std::filesystem::remove(L"/home/welschj/temp/testafx.txt");
}
#endif

#ifdef _TEST
void Synchronize::startLogging(LogLevel llevel, const wstring& lfname)
{
    #ifdef _UNIX
        std::call_once(removeTestFlag, removeTestFile);
        if (lfname.empty())
            Logging::instance().setLogFilePathName(L"/home/welschj/temp/testafx.txt");
        else
            Logging::instance().setLogFilePathName(lfname);
    #else
        if (lfname.empty())
            Logging::instance().setLogFilePathName(L"C:\\temp\\testafx.txt");
        else
            Logging::instance().setLogFilePathName(lfname);
    #endif
    Logging::instance().setLogLevel(llevel);
}
#endif

void Synchronize::stop()
{
   LOG_DEBUG(L"Synchronize::stop()");
   if (hasInstance()) {
       instance().stopRequest_ = true;
       instance().cv_.notify_all();
   }
   stop_ = true;
}
#ifdef _TEST
void Synchronize::start()
{
    LOG_DEBUG(L"Synchronize::start()");
    stop_ = false;
}
#endif
void Synchronize::setProgressMessage(const std::wstring& m, int64_t sourcesize)
{
    Synchronize* this_ptr = this_.load();
    if (this_ptr)
        this_ptr->messages_.push(SyncMessage(m, this_ptr->countFiles_, sourcesize));
}

SyncMessage Synchronize::getMessage()
{
    SyncMessage retval;
    if (!messages_.empty()) {
        messages_.pop(retval);
        while (!messages_.empty())
            messages_.pop();  //  delete superfluous messages
    }
    return retval;
}

std::size_t Synchronize::countFiles() const
{
    return countFiles_;
}

bool Synchronize::pauseElapsed(const SyncItem& si)
{
    bool retval = std::chrono::steady_clock::now() >= (si.lastSync() + std::chrono::seconds(si.syncCycle()));
    std::wstringstream sout;
    sout << L"Synchronize::pauseElapsed() for " << si.name() << L": " << std::boolalpha  << retval;
    LOG_DEBUG(sout.str());
    if (!retval) {
        LOG_DEBUG2("now: ", std::chrono::steady_clock::now());
        LOG_DEBUG2("lastSync: ", si.lastSync());
    }
    return retval;
}

uint16_t Synchronize::getDriveCheckCacheInterval() const
{
    const uint16_t defaultInterval = 2;
    return defaultInterval;
}

bool Synchronize::pathExists(const std::wstring& pathname) const
{
    LOG_DEBUG(L"pathExists()");
    namespace fs = std::filesystem;
    fs::path path(pathname);
    std::error_code ec;
    return fs::exists(path, ec);
}

SyncControler& Synchronize::syncControler() const
{
    return *syncControler_;
}

bool Synchronize::isSshTargetDir(const std::wstring& target)
{
    // sshIsDirectory() opens a full SSH connection every call; in continuous
    // sync (cycle down to 1s) that reconnected to the server on every single
    // pass just for this check. Cache per target with a short TTL - a stale
    // answer (remote dir created/removed, connection hiccup) heals on expiry.
    // Same self-healing pattern as CheckPath's drive cache.
    const auto now = std::chrono::steady_clock::now();
    auto it = sshTargetDirCache_.find(target);
    if (it != sshTargetDirCache_.end() && now - it->second.checked < std::chrono::seconds(60))
        return it->second.isDir;
    const bool isDir = sshIsDirectory(target);
    sshTargetDirCache_[target] = { isDir, now };
    return isDir;
}

void Synchronize::syncItem(SyncItem& i)
{
    LOG_DEBUG2(L"Synchronize::syncItem: ", i.name());
    if (!i.enabled())
        return;
    LOG_DEBUG2(L"Synchronize::operator()() Start of ", i.name());
    if (!pauseElapsed(i))
        return;
    // Counting passes touch nothing and have nothing to prove; without a
    // directory to write to there is nowhere to put a proof either.
    if (!i.backupProof() || syncType_ == SyncType::CountFiles
        || Config::instance().settings().backupProofPath().empty())
        runSyncItem(i);
    else
        proveSyncItem(i);
}

// A drive that is not mounted or a server that does not answer makes the item
// sit still (see runSyncItem) - but a proof that keeps quiet about it documents
// nothing at all for an item whose source vanished. Only the change to
// "unreachable" is reported: an item that stays unreachable would otherwise add
// one entry per cycle, down to one per second. An item not seen before counts
// as reachable, so the first failing pass is the one that gets reported.
void Synchronize::reportUnreachable(const SyncItem& i)
{
    bool& reachable = itemReachable_.try_emplace(i.name(), true).first->second;
    if (std::exchange(reachable, false))
        reportSyncError(L"Quelle oder Ziel nicht erreichbar: " + i.source() + L" -> " + i.target());
}

// The guard covers runSyncItem() as a whole, its catch blocks included: a pass
// that failed before it copied anything is exactly the one worth proving.
void Synchronize::proveSyncItem(SyncItem& i)
{
    SyncReport report;
    const std::size_t filesBefore = countFiles_;
    {
        SyncReportGuard guard(&report);
        runSyncItem(i);
    }
    const std::size_t files = countFiles_ - filesBefore;
    // A pass that found nothing to do is not evidence of anything. Recording
    // one entry per cycle - down to one second - would bury the passes that
    // did something under millions that did not.
    if (files == 0 && report.empty())
        return;
    writeBackupProof(Config::instance().settings().backupProofPath(), i, files, report);
}

void Synchronize::runSyncItem(SyncItem& i)
{
    // checkPath_/fs::is_directory use the throwing fs:: overloads and can raise
    // filesystem_error (e.g. permission denied); one bad item must not abort
    // the whole sync loop the way SyncDir/SyncFile/SyncTree already avoid it.
    try {
        if (!checkPath_(i.source(), i.target(), i.sshPathType() != SSHPATH::none)) {
            reportUnreachable(i);
            return;
        }
        itemReachable_[i.name()] = true;
        if (i.syncItemType() == SyncItemType::Dir) {
            SyncDir sd(i, countFiles_, syncType_==SyncType::CountFiles);
            sd();
        }
        else if (i.syncItemType() == SyncItemType::File) {
            // fs::is_directory only ever sees the local filesystem, so an SSH
            // target ("account:/remote/dir") always looked like "not a directory"
            // to it - the source filename never got appended, so the copy wrote
            // (or, worse, tried to overwrite) the directory itself.
            bool isTargetDir = i.sshTarget() ? isSshTargetDir(i.target()) : fs::is_directory(withoutVolumeSpec(i.target()));
            SyncFile(i, countFiles_, syncType_ == SyncType::CountFiles, isTargetDir)();
        }
        else if (i.syncItemType() == SyncItemType::Tree) {
            SyncTree sync(i, countFiles_, syncType_ == SyncType::CountFiles);
            sync();
        }
    }
    catch (std::filesystem::filesystem_error const& ex) {
        handleLastError(ex);
        reportSyncError(string_to_wstring(ex.what()));
        return;
    }
    catch (std::exception& ex) {
        LOG_ERROR2(L"Synchronize::syncItem fehlgeschlagen fuer: ", i.name())
        LOG_ERROR(string_to_wstring(ex.what()))
        reportSyncError(string_to_wstring(ex.what()));
        return;
    }
    catch (...) {
        LOG_ERROR2(L"Unbekannter Fehler in Synchronize::syncItem fuer: ", i.name())
        reportSyncError(L"Unbekannter Fehler");
        return;
    }
    if (syncType_ != SyncType::CountFiles)
        // i is this pass's snapshot copy (see operator()()), not the item held by
        // Config - update lastSync there so the next pass's snapshot sees it.
        Config::instance().refreshLastSync(i.name());
}

void Synchronize::operator()()
{
    LOG_DEBUG(L"Synchronize::operator()()");
    this_.store(this);
    try {
        do {
#ifdef _TEST
            ++loopIterations_;
#endif
            // GUI and daemon share this engine and this configuration, so only
            // one of them may pass here at a time.
            if (!arbiter_.acquire(stopRequest_))
                break;
            // Snapshot copy: syncItem() can run long (large file copies) while
            // holding a SyncItem by value in SyncDir/SyncFile/SyncTree, so it must
            // not depend on Config's live vector staying stable for that long.
            for (auto& i: Config::instance().syncitems()) {
                if (stopRequest_.load())
                    break;
                syncItem(i);
            }
            // The daemon hands the turn back after every pass, which is what
            // lets a GUI sync take over; the GUI keeps it until its own sync
            // ends, and that is what makes the daemon pause for that whole time.
            // Granularity is one item: a copy already under way finishes first.
            if (role_ == SyncRole::Background)
                arbiter_.release();
            if (syncType_ == SyncType::Continuous && !stop_) {
                std::unique_lock<std::mutex> lock(cvMutex_);
                cv_.wait_for(lock, std::chrono::seconds(1), [this]{ return stopRequest_.load() || stop_.load(); });
            }
        } while (syncType_ == SyncType::Continuous && !stop_);
    }
    catch (std::exception& ex) {
        LOG_FATAL(string_to_wstring(ex.what()))
	}
    catch (...) {
        LOG_FATAL(L"Unknown error caught in Synchronize::operator()()")
    }
    arbiter_.release();

#ifdef _WXWIDGETS
    if (syncControler_ != nullptr && !syncControler_->syncFinished()) {
        syncControler_->countFiles(countFiles_);
        syncControler_->setSyncFinished();
    }
#endif
    LOG_DEBUG(L"End of Synchronize::operator()()");
}
