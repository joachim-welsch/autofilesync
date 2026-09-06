#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <set>
#include <map>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include "Global.h"
#include "Config.h"
#include "ThreadSaveStack.h"
#include "SyncMessage.h"
#include "CheckPath.h"
#include "SyncArbiter.h"
#include <Logging.h>

namespace AFS_Core {

class SyncControler;
class SyncItem;

class Synchronize {
    using MessageStack = ThreadSaveStack<SyncMessage>;

    static std::atomic<Synchronize*>    this_;
    static std::atomic_bool stop_;
    std::atomic<SyncType>   syncType_;
    std::size_t             countFiles_ = 0;
    SyncControler*          syncControler_ = nullptr;
    std::thread             thread_;
    std::atomic_bool        stopRequest_{false};
    MessageStack            messages_;
    CheckPath               checkPath_;
    std::mutex              cvMutex_;
    std::condition_variable cv_;
    // Defaults keep the test-only default constructor usable; the real
    // constructor sets both from its role argument.
    SyncRole                role_ = SyncRole::Foreground;
    SyncArbiter             arbiter_{SyncRole::Foreground};
#ifdef _TEST
    std::atomic<int>        loopIterations_{0}; // counts do-while passes of operator()(), for race regression tests
#endif

    struct SshTargetDirCacheEntry {
        bool isDir;
        std::chrono::steady_clock::time_point checked;
    };
    // Only touched by the worker thread (syncItem), so no lock is needed.
    std::map<std::wstring, SshTargetDirCacheEntry> sshTargetDirCache_;
    // Last known reachability per item name, so an item that stays unreachable
    // is reported once instead of once per pass. Same thread as above.
    std::map<std::wstring, bool> itemReachable_;

    bool hasControler() const;
    uint16_t getDriveCheckCacheInterval() const;
    bool isSshTargetDir(const std::wstring& target);
    void syncItem(SyncItem& i);
    void runSyncItem(SyncItem& i);
    void reportUnreachable(const SyncItem& i);
    void proveSyncItem(SyncItem& i);
#ifdef _TEST
public:
#endif
    bool pauseElapsed(const SyncItem&);
    bool pathExists(const std::wstring& pathname) const;
    Synchronize() = default;
    Synchronize(SyncType syncType, SyncControler* sc = nullptr, SyncRole role = SyncRole::Foreground);
    bool stop_requested() const;
protected:
    static void stop();
    std::size_t countFiles() const;
#ifdef _TEST
public:
    static void start();
#endif
    void operator()();
public:
    static Synchronize* create(SyncType syncType, SyncControler* sc = nullptr, SyncRole role = SyncRole::Foreground);
    static bool hasInstance();
    static Synchronize& instance();
	static void destroy();
    virtual ~Synchronize();

    #ifdef _TEST
        void startLogging(LogLevel, const std::wstring& lfname);
        int loopIterations() const { return loopIterations_; }
    #endif
    static void setProgressMessage(const std::wstring&, int64_t sourcesize);
    SyncMessage getMessage();
    SyncControler& syncControler() const;
    static bool stopRequested();
	friend class SyncControler;
};

//#####################################################################

inline
bool Synchronize::stop_requested() const {
    return stopRequest_.load();
}

} // namespace AFS_Core
