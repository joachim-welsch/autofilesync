#pragma once

#include <atomic>

// Cross process arbitration between the GUI and the backgroundsync daemon: both
// run the same Synchronize engine over the same configuration, so only one of
// them may synchronise at a time. The GUI has priority - the daemon pauses
// between its passes for as long as a GUI synchronisation is running.
enum class SyncRole { Foreground, Background };

class SyncArbiter {
public:
    explicit SyncArbiter(SyncRole role): role_(role) {}
    SyncArbiter(const SyncArbiter&) = delete;
    SyncArbiter& operator=(const SyncArbiter&) = delete;
    ~SyncArbiter();
    // Blocks until this process may synchronise; calling it again while already
    // held returns immediately. Returns false when abort turns true first, so a
    // stop request is never delayed by whatever the other process is doing.
    bool acquire(const std::atomic_bool& abort);
    void release();
private:
    bool otherSideActive() const;
    void publishActive();
    void withdrawActive();

    SyncRole role_;
    bool held_ = false;
#ifdef WIN32
    // A HANDLE to the Global\ event announcing that this side synchronises. It
    // is deliberately kept as a plain pointer so that no Windows header leaks
    // into everything that synchronises.
    void* activeEvent_ = nullptr;
#else
    int activeLockFd_ = -1;
#endif
};
