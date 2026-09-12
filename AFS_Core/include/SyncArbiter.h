
#pragma once

#include <atomic>

namespace AFS_Core {

enum class SyncRole { Foreground, Background };  // [SyncArbiter.md#SyncArbiter]

class SyncArbiter {
public:
    explicit SyncArbiter(SyncRole role): role_(role) {}
    SyncArbiter(const SyncArbiter&) = delete;
    SyncArbiter& operator=(const SyncArbiter&) = delete;
    ~SyncArbiter();
    bool acquire(const std::atomic_bool& abort);  // [SyncArbiter.md#acquire]
    void release();
private:
    bool other_side_active() const;
    void publish_active();
    void withdraw_active();

    SyncRole role_;
    bool held_ = false;
#ifdef WIN32
    void* active_event_ = nullptr;  // [SyncArbiter.md#active_event_]
#else
    int active_lock_fd_ = -1;
#endif
};

} // namespace AFS_Core
