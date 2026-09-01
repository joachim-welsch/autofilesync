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

namespace {
// Slices the waiting so that a stop request is noticed quickly without polling
// the other process more often than necessary.
constexpr auto pollInterval = std::chrono::milliseconds(200);
// The daemon waits for the GUI without a limit: that is the requested pause,
// and a GUI that dies takes its announcement with it (see below). The GUI only
// waits for the daemon to finish its current pass, so a limit here merely keeps
// a daemon that cannot be inspected - one running under a different account -
// from blocking the user's own synchronisation for good.
constexpr auto foregroundWaitLimit = std::chrono::seconds(60);

#ifdef WIN32
// Presence of the event, not its signal state, carries the message. Windows
// destroys the object once the last handle is gone, and it closes the handles
// of a process that dies, so a crashed GUI cannot leave the daemon paused
// forever - which a flag that outlives its owner would.
const wchar_t* eventName(SyncRole role)
{
    return role == SyncRole::Foreground ? L"Global\\AutoFileSyncForegroundSync" : L"Global\\AutoFileSyncBackgroundSync";
}
#else
std::string lockPath(SyncRole role)
{
    const std::string suffix = role == SyncRole::Foreground ? "foreground" : "background";
    // Per user: /tmp is shared, and daemon and GUI only need to meet when they
    // run under the same account - which they must anyway to share the
    // configuration under the user's profile.
    return "/tmp/autofilesync-" + std::to_string(getuid()) + "-" + suffix + ".lock";
}
#endif

SyncRole otherRole(SyncRole role)
{
    return role == SyncRole::Foreground ? SyncRole::Background : SyncRole::Foreground;
}
}

void SyncArbiter::publishActive()
{
#ifdef WIN32
    activeEvent_ = CreateEventW(nullptr, TRUE, TRUE, eventName(role_));
    if (activeEvent_ == nullptr)
        LOG_WARNING2(L"SyncArbiter: Ankuendigung fehlgeschlagen, Fehler ", std::to_wstring(GetLastError()));
#else
    activeLockFd_ = open(lockPath(role_).c_str(), O_RDWR | O_CREAT, 0600);
    if (activeLockFd_ < 0) {
        LOG_WARNING2(L"SyncArbiter: Sperrdatei nicht anlegbar, Fehler ", std::to_wstring(errno));
        return;
    }
    // A failure here means a second process of the same role synchronises. That
    // is outside what this arbitration solves (the GUI already refuses a second
    // sync of its own), and refusing to synchronise would be the worse answer.
    if (flock(activeLockFd_, LOCK_EX | LOCK_NB) != 0)
        LOG_WARNING(L"SyncArbiter: eigene Sperre bereits vergeben, Synchronisation laeuft trotzdem weiter.");
#endif
}

void SyncArbiter::withdrawActive()
{
#ifdef WIN32
    if (activeEvent_ != nullptr) {
        CloseHandle(activeEvent_);
        activeEvent_ = nullptr;
    }
#else
    if (activeLockFd_ >= 0) {
        flock(activeLockFd_, LOCK_UN);
        close(activeLockFd_);
        activeLockFd_ = -1;
    }
#endif
}

bool SyncArbiter::otherSideActive() const
{
#ifdef WIN32
    HANDLE other = OpenEventW(SYNCHRONIZE, FALSE, eventName(otherRole(role_)));
    if (other != nullptr) {
        CloseHandle(other);
        return true;
    }
    // ERROR_ACCESS_DENIED means the object is there but belongs to another
    // account, so the other side is synchronising; only its absence is proof
    // that nobody is.
    return GetLastError() != ERROR_FILE_NOT_FOUND;
#else
    const int other = open(lockPath(otherRole(role_)).c_str(), O_RDWR | O_CREAT, 0600);
    if (other < 0)
        return false;
    // Taking the lock is the only way to ask whether it is free; it is given
    // back at once, so the owner is delayed by no more than this probe.
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
        // Both sides announce before they look, which is what rules out a
        // simultaneous start: whoever checks second must see the first one.
        // Only the reaction differs - the daemon steps back and retries while
        // the GUI keeps its announcement up and waits, so the GUI always wins.
        while (!abort.load()) {
            publishActive();
            if (!otherSideActive()) {
                held_ = true;
                return true;
            }
            withdrawActive();
            std::this_thread::sleep_for(pollInterval);
        }
        return false;
    }
    publishActive();
    const auto deadline = std::chrono::steady_clock::now() + foregroundWaitLimit;
    while (otherSideActive()) {
        if (abort.load()) {
            withdrawActive();
            return false;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            LOG_WARNING(L"SyncArbiter: Hintergrunddienst gibt die Synchronisation nicht frei, GUI synchronisiert trotzdem.");
            break;
        }
        std::this_thread::sleep_for(pollInterval);
    }
    held_ = true;
    return true;
}

void SyncArbiter::release()
{
    if (!held_)
        return;
    withdrawActive();
    held_ = false;
}

SyncArbiter::~SyncArbiter()
{
    release();
}
