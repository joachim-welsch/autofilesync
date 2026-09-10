#include "pch.h"
#include "SyncArbiter.h"
#include <chrono>
#include <thread>
#include <Logging.h>
#ifdef WIN32
    #include <Windows.h>
#else
    #include <fcntl.h>
    #include <sys/file.h>
    #include <unistd.h>
    #include <cerrno>
    #include <string>
#endif

namespace AFS_Core {

namespace {
constexpr auto POLL_INTERVAL = std::chrono::milliseconds(200);
constexpr auto FOREGROUND_WAIT_LIMIT = std::chrono::seconds(60);

#ifdef WIN32
const wchar_t* event_name(SyncRole role)
{
    return role == SyncRole::Foreground ? L"Global\\AutoFileSyncForegroundSync" : L"Global\\AutoFileSyncBackgroundSync";
}
#else
std::string lock_path(SyncRole role)
{
    const std::string suffix = role == SyncRole::Foreground ? "foreground" : "background";
    // Per user: /tmp is shared, and daemon and GUI only need to meet when they
    // run under the same account - which they must anyway to share the
    // configuration under the user's profile.
    return "/tmp/autofilesync-" + std::to_string(getuid()) + "-" + suffix + ".lock";
}
#endif

SyncRole other_role(SyncRole role)
{
    return role == SyncRole::Foreground ? SyncRole::Background : SyncRole::Foreground;
}
}

void SyncArbiter::publish_active()
{
#ifdef WIN32
    active_event_ = CreateEventW(nullptr, TRUE, TRUE, event_name(role_));
    if (active_event_ == nullptr)
        log_warning(L"SyncArbiter: Ankuendigung fehlgeschlagen, Fehler ", std::to_wstring(GetLastError()));
#else
    active_lock_fd_ = open(lock_path(role_).c_str(), O_RDWR | O_CREAT, 0600);
    if (active_lock_fd_ < 0) {
        log_warning(L"SyncArbiter: Sperrdatei nicht anlegbar, Fehler ", std::to_wstring(errno));
        return;
    }
    if (flock(active_lock_fd_, LOCK_EX | LOCK_NB) != 0)
        log_warning(L"SyncArbiter: eigene Sperre bereits vergeben, Synchronisation laeuft trotzdem weiter.");
#endif
}

void SyncArbiter::withdraw_active()
{
#ifdef WIN32
    if (active_event_ != nullptr) {
        CloseHandle(active_event_);
        active_event_ = nullptr;
    }
#else
    if (active_lock_fd_ >= 0) {
        flock(active_lock_fd_, LOCK_UN);
        close(active_lock_fd_);
        active_lock_fd_ = -1;
    }
#endif
}

bool SyncArbiter::other_side_active() const
{
#ifdef WIN32
    HANDLE other = OpenEventW(SYNCHRONIZE, FALSE, event_name(other_role(role_)));
    if (other != nullptr) {
        CloseHandle(other);
        return true;
    }
    return GetLastError() != ERROR_FILE_NOT_FOUND;
#else
    const int other = open(lock_path(other_role(role_)).c_str(), O_RDWR | O_CREAT, 0600);
    if (other < 0)
        return false;
    const bool free = flock(other, LOCK_EX | LOCK_NB) == 0;
    if (free)
        flock(other, LOCK_UN);
    close(other);
    return !free;
#endif
}

bool SyncArbiter::acquire(const std::atomic_bool& abort)
{
    if (held_)
        return true;
    if (role_ == SyncRole::Background) {
        while (!abort.load()) {
            publish_active();
            if (!other_side_active()) {
                held_ = true;
                return true;
            }
            withdraw_active();
            std::this_thread::sleep_for(POLL_INTERVAL);
        }
        return false;
    }
    publish_active();
    const auto deadline = std::chrono::steady_clock::now() + FOREGROUND_WAIT_LIMIT;
    while (other_side_active()) {
        if (abort.load()) {
            withdraw_active();
            return false;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            log_warning(L"SyncArbiter: Hintergrunddienst gibt die Synchronisation nicht frei, GUI synchronisiert trotzdem.");
            break;
        }
        std::this_thread::sleep_for(POLL_INTERVAL);
    }
    held_ = true;
    return true;
}

void SyncArbiter::release()
{
    if (!held_)
        return;
    withdraw_active();
    held_ = false;
}

SyncArbiter::~SyncArbiter()
{
    release();
}

} // namespace AFS_Core
