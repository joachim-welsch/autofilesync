/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of AFS_Core, the synchronisation engine of AutoFileSync.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
#include "pch.h"
#include "ChangeWatcher.h"
#include "Logging.h"
#include "Translation.h"
#include "Util.h"
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <system_error>
#include <thread>
#if defined(__linux__)
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unistd.h>
#elif defined(_MACOS)
#include <CoreServices/CoreServices.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace AFS_Core {

namespace fs = std::filesystem;
using std::wstring;

void ChangeWatcher::changed(const wstring& item)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = items_.find(item);
    if (found != items_.end() && found->second.watching)
        found->second.last_change = std::chrono::steady_clock::now();
}

void ChangeWatcher::changed_everywhere()  // [ChangeWatcher.md#overflow]
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [item, entry] : items_)
        if (entry.watching)
            entry.last_change = std::chrono::steady_clock::now();
}

void ChangeWatcher::lost(const wstring& item, const fs::path& dir)  // [ChangeWatcher.md#watching]
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = items_.find(item);
        if (found == items_.end() || !found->second.watching)
            return;
        found->second.watching = false;
    }
    log_info(translation::translate("watching_unavailable", {item, dir.wstring()}));
}

bool ChangeWatcher::watching(const wstring& item) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = items_.find(item);
    return found != items_.end() && found->second.watching;
}

std::optional<ChangeWatcher::TimePoint> ChangeWatcher::last_change(const wstring& item) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = items_.find(item);
    return found == items_.end() ? std::nullopt : found->second.last_change;
}

void ChangeWatcher::clear(const wstring& item)  // [ChangeWatcher.md#clear]
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = items_.find(item);
    if (found != items_.end())
        found->second.last_change.reset();
}

#if defined(__linux__)

struct ChangeWatcher::Backend {  // [ChangeWatcher.md#Backend]
    struct Watch {
        wstring item;
        fs::path dir;
        bool recursive;
    };
    static constexpr uint32_t EVENTS = IN_CREATE | IN_DELETE | IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR;
    ChangeWatcher& owner;
    int fd = -1;
    int stop_fd = -1;
    std::map<int, Watch> watches;  // written by the constructor, then by the thread alone
    std::thread thread;

    Backend(ChangeWatcher& watcher, const std::vector<WatchedDir>& dirs)
        : owner(watcher)
    {
        fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        stop_fd = eventfd(0, EFD_CLOEXEC);
        if (fd < 0 || stop_fd < 0) {
            for (const WatchedDir& watched : dirs)
                owner.lost(watched.item, watched.dir);
            return;
        }
        for (const WatchedDir& watched : dirs)
            add(watched.item, watched.dir, watched.recursive);
        thread = std::thread([this] { run(); });
    }

    ~Backend()
    {
        if (thread.joinable()) {
            const uint64_t one = 1;
            [[maybe_unused]] const ssize_t written = write(stop_fd, &one, sizeof one);
            thread.join();
        }
        if (fd >= 0)
            close(fd);
        if (stop_fd >= 0)
            close(stop_fd);
    }

    static void collect(const fs::path& dir, std::vector<fs::path>& dirs)
    {
        std::error_code ec;
        for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
            if (it->is_directory(ec) && !it->is_symlink(ec)) {
                dirs.push_back(it->path());
                collect(it->path(), dirs);
            }
    }

    bool add(const wstring& item, const fs::path& dir, bool recursive)  // [ChangeWatcher.md#inotify-per-directory]
    {
        std::vector<fs::path> dirs{dir};
        if (recursive)
            collect(dir, dirs);  // listed before any watch exists: the listing itself updates access times, which a watch would report
        for (const fs::path& watched : dirs) {
            const int wd = inotify_add_watch(fd, watched.c_str(), EVENTS);
            if (wd < 0) {
                owner.lost(item, watched);
                return false;
            }
            watches[wd] = {item, watched, recursive};
        }
        return true;
    }

    void run()
    {
        std::vector<char> buffer(64 * 1024);
        pollfd fds[2] = {{fd, POLLIN, 0}, {stop_fd, POLLIN, 0}};
        while (poll(fds, 2, -1) >= 0 || errno == EINTR) {
            if (fds[1].revents & POLLIN)
                return;
            if (!(fds[0].revents & POLLIN))
                continue;
            const ssize_t length = read(fd, buffer.data(), buffer.size());
            for (ssize_t offset = 0; offset < length;) {
                const auto* event = reinterpret_cast<const inotify_event*>(buffer.data() + offset);
                handle(*event);
                offset += static_cast<ssize_t>(sizeof(inotify_event) + event->len);
            }
        }
    }

    void handle(const inotify_event& event)
    {
        if (event.mask & IN_Q_OVERFLOW) {  // [ChangeWatcher.md#overflow]
            owner.changed_everywhere();
            return;
        }
        const auto found = watches.find(event.wd);
        if (found == watches.end())
            return;
        const Watch watch = found->second;
        owner.changed(watch.item);
        if (event.mask & IN_IGNORED)
            watches.erase(found);
        else if (watch.recursive && (event.mask & IN_ISDIR) && (event.mask & (IN_CREATE | IN_MOVED_TO)) && event.len > 0)
            add(watch.item, watch.dir / event.name, true);  // a directory that appeared is watched from now on
    }
};

bool ChangeWatcher::available()
{
    return true;
}

#elif defined(_MACOS)

struct ChangeWatcher::Backend {  // [ChangeWatcher.md#Backend]
    struct Stream {
        ChangeWatcher* owner;
        WatchedDir watched;
        FSEventStreamRef ref = nullptr;
    };
    ChangeWatcher& owner;
    std::vector<std::unique_ptr<Stream>> streams;
    CFRunLoopRef loop = nullptr;
    std::thread thread;
    std::mutex ready_mutex;
    std::condition_variable ready_cv;
    bool ready = false;

    static void callback(ConstFSEventStreamRef, void* info, size_t count, void* paths, const FSEventStreamEventFlags flags[], const FSEventStreamEventId[])
    {
        Stream* stream = static_cast<Stream*>(info);
        char** changed = static_cast<char**>(paths);
        for (size_t i = 0; i < count; ++i) {
            if (flags[i] & (kFSEventStreamEventFlagMustScanSubDirs | kFSEventStreamEventFlagKernelDropped | kFSEventStreamEventFlagUserDropped)) {
                stream->owner->changed_everywhere();  // [ChangeWatcher.md#overflow]
                continue;
            }
            if (stream->watched.recursive || inside(changed[i], stream->watched.dir))
                stream->owner->changed(stream->watched.item);
        }
    }

    static bool inside(const char* reported, const fs::path& dir)  // [ChangeWatcher.md#fsevents-paths]
    {
        const fs::path raw(reported);
        const fs::path path = raw.has_filename() ? raw : raw.parent_path();
        return path == dir || path.parent_path() == dir;
    }

    Backend(ChangeWatcher& watcher, const std::vector<WatchedDir>& dirs)
        : owner(watcher)
    {
        for (const WatchedDir& configured : dirs) {
            std::error_code ec;
            const WatchedDir watched{configured.item, fs::canonical(configured.dir, ec), configured.recursive};  // [ChangeWatcher.md#fsevents-paths]
            if (ec || !fs::is_directory(watched.dir, ec)) {
                owner.lost(configured.item, configured.dir);
                continue;
            }
            auto stream = std::make_unique<Stream>(Stream{&owner, watched});
            CFStringRef path = CFStringCreateWithCString(nullptr, watched.dir.c_str(), kCFStringEncodingUTF8);
            CFArrayRef paths = CFArrayCreate(nullptr, reinterpret_cast<const void**>(&path), 1, &kCFTypeArrayCallBacks);
            FSEventStreamContext context{0, stream.get(), nullptr, nullptr, nullptr};
            stream->ref = FSEventStreamCreate(nullptr, &callback, &context, paths, kFSEventStreamEventIdSinceNow, 1.0,
                kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer);
            CFRelease(paths);
            CFRelease(path);
            if (stream->ref == nullptr)
                owner.lost(watched.item, watched.dir);
            else
                streams.push_back(std::move(stream));
        }
        if (streams.empty())
            return;
        thread = std::thread([this] { run(); });
        std::unique_lock<std::mutex> lock(ready_mutex);
        ready_cv.wait(lock, [this] { return ready; });
    }

    ~Backend()
    {
        if (thread.joinable()) {
            CFRunLoopStop(loop);
            thread.join();
        }
        for (const auto& stream : streams) {  // after the loop ended no callback runs any more
            FSEventStreamStop(stream->ref);
            FSEventStreamInvalidate(stream->ref);
            FSEventStreamRelease(stream->ref);
        }
    }

    void run()
    {
        loop = CFRunLoopGetCurrent();
        for (const auto& stream : streams) {
            FSEventStreamScheduleWithRunLoop(stream->ref, loop, kCFRunLoopDefaultMode);
            if (!FSEventStreamStart(stream->ref))
                owner.lost(stream->watched.item, stream->watched.dir);
        }
        {
            std::lock_guard<std::mutex> lock(ready_mutex);
            ready = true;
        }
        ready_cv.notify_all();
        CFRunLoopRun();
    }
};

bool ChangeWatcher::available()
{
    return true;
}

#elif defined(_WIN32)

struct ChangeWatcher::Backend {  // [ChangeWatcher.md#Backend]
    struct Watch {
        WatchedDir watched;
        HANDLE dir = INVALID_HANDLE_VALUE;
        HANDLE event = nullptr;
        OVERLAPPED overlapped{};
        std::vector<char> buffer = std::vector<char>(64 * 1024);
    };
    static constexpr DWORD FILTER = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES
        | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;
    static constexpr std::size_t MAX_WATCHES = MAXIMUM_WAIT_OBJECTS - 1;  // [ChangeWatcher.md#windows-wait-limit]
    ChangeWatcher& owner;
    std::vector<std::unique_ptr<Watch>> watches;
    HANDLE stop_event = nullptr;
    std::thread thread;

    Backend(ChangeWatcher& watcher, const std::vector<WatchedDir>& dirs)
        : owner(watcher)
    {
        stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        for (const WatchedDir& watched : dirs) {
            if (stop_event == nullptr || watches.size() >= MAX_WATCHES) {
                owner.lost(watched.item, watched.dir);
                continue;
            }
            auto watch = std::make_unique<Watch>();
            watch->watched = watched;
            watch->dir = CreateFileW(watched.dir.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
            watch->event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            watch->overlapped.hEvent = watch->event;
            if (watch->dir == INVALID_HANDLE_VALUE || watch->event == nullptr || !issue(*watch)) {
                owner.lost(watched.item, watched.dir);
                release(*watch);
                continue;
            }
            watches.push_back(std::move(watch));
        }
        if (!watches.empty())
            thread = std::thread([this] { run(); });
    }

    ~Backend()
    {
        if (thread.joinable()) {
            SetEvent(stop_event);
            thread.join();
        }
        for (const auto& watch : watches)
            release(*watch);
        if (stop_event != nullptr)
            CloseHandle(stop_event);
    }

    static void release(Watch& watch)
    {
        if (watch.dir != INVALID_HANDLE_VALUE) {
            DWORD ignored = 0;
            if (CancelIoEx(watch.dir, &watch.overlapped) || GetLastError() != ERROR_NOT_FOUND)
                GetOverlappedResult(watch.dir, &watch.overlapped, &ignored, TRUE);  // [ChangeWatcher.md#cancel-before-free]
            CloseHandle(watch.dir);
            watch.dir = INVALID_HANDLE_VALUE;
        }
        if (watch.event != nullptr) {
            CloseHandle(watch.event);
            watch.event = nullptr;
        }
    }

    static bool issue(Watch& watch)
    {
        return ReadDirectoryChangesW(watch.dir, watch.buffer.data(), static_cast<DWORD>(watch.buffer.size()), watch.watched.recursive ? TRUE : FALSE,
            FILTER, nullptr, &watch.overlapped, nullptr) != 0;
    }

    void run()
    {
        std::vector<HANDLE> handles{stop_event};
        for (const auto& watch : watches)
            handles.push_back(watch->event);
        for (;;) {
            const DWORD signalled = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, INFINITE);
            if (signalled == WAIT_OBJECT_0 || signalled == WAIT_FAILED)
                return;
            const std::size_t index = signalled - WAIT_OBJECT_0 - 1;
            if (index >= watches.size())
                continue;
            Watch& watch = *watches[index];
            DWORD bytes = 0;
            if (!GetOverlappedResult(watch.dir, &watch.overlapped, &bytes, FALSE)) {
                owner.lost(watch.watched.item, watch.watched.dir);
                continue;
            }
            owner.changed(watch.watched.item);  // [ChangeWatcher.md#overflow] zero bytes means the buffer overflowed, which is a change as well
            if (!issue(watch))
                owner.lost(watch.watched.item, watch.watched.dir);
        }
    }
};

bool ChangeWatcher::available()
{
    return true;
}

#else

struct ChangeWatcher::Backend {
    Backend(ChangeWatcher& owner, const std::vector<WatchedDir>& dirs)
    {
        for (const WatchedDir& watched : dirs)
            owner.lost(watched.item, watched.dir);
    }
};

bool ChangeWatcher::available()
{
    return false;
}

#endif

ChangeWatcher::ChangeWatcher(const std::vector<WatchedDir>& dirs)
{
    for (const WatchedDir& watched : dirs)
        items_[watched.item].watching = true;
    backend_ = std::make_unique<Backend>(*this, dirs);
}

ChangeWatcher::~ChangeWatcher() = default;

} // namespace AFS_Core
