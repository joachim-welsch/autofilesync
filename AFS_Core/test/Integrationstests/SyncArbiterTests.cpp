#include "pch.h"
#include "SyncArbiter.h"
#include <atomic>
#include <chrono>
#include <thread>

using namespace AFS_Core;

using namespace std::chrono_literals;

namespace {
// Two arbiters in one process see each other exactly as two processes do: the
// Windows events live in the Global\ namespace and flock treats two open file
// descriptors of the same file independently, even within one process.
std::chrono::milliseconds time_acquire(SyncArbiter& arbiter, const std::atomic_bool& abort, bool& acquired)
{
    const auto start = std::chrono::steady_clock::now();
    acquired = arbiter.acquire(abort);
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
}
}

// The requirement: while the GUI synchronises, the daemon pauses.
TEST(SyncArbiterTests, BackgroundWaitsWhileForegroundHolds)
{
    std::atomic_bool abort{false};
    SyncArbiter foreground(SyncRole::Foreground);
    bool acquired = false;
    ASSERT_LT(time_acquire(foreground, abort, acquired).count(), 500);
    ASSERT_TRUE(acquired);

    std::atomic_bool background_entered{false};
    std::thread daemon([&]() {
        SyncArbiter background(SyncRole::Background);
        std::atomic_bool no_abort{false};
        background.acquire(no_abort);
        background_entered = true;
        background.release();
    });
    std::this_thread::sleep_for(600ms);
    EXPECT_FALSE(background_entered.load()) << "Der Hintergrunddienst haette pausieren muessen.";

    foreground.release();
    daemon.join();
    EXPECT_TRUE(background_entered.load()) << "Nach dem Ende der GUI-Synchronisation muss der Dienst weiterlaufen.";
}

// The other direction: only one synchronisation at a time means the GUI has to
// let a pass that is already running finish.
TEST(SyncArbiterTests, ForegroundWaitsForRunningBackgroundPass)
{
    std::atomic_bool abort{false};
    SyncArbiter background(SyncRole::Background);
    bool acquired = false;
    ASSERT_TRUE(background.acquire(abort));

    std::thread releaser([&]() {
        std::this_thread::sleep_for(700ms);
        background.release();
    });
    SyncArbiter foreground(SyncRole::Foreground);
    const auto waited = time_acquire(foreground, abort, acquired);
    releaser.join();

    EXPECT_TRUE(acquired);
    EXPECT_GE(waited.count(), 500) << "Die GUI haette auf den laufenden Durchlauf warten muessen.";
    foreground.release();
}

// A stop request must not be delayed by whatever the other side is doing, or
// stopping a sync would hang for as long as the other process keeps going.
TEST(SyncArbiterTests, AbortEndsTheWaitInsteadOfBlocking)
{
    std::atomic_bool abort{false};
    SyncArbiter foreground(SyncRole::Foreground);
    ASSERT_TRUE(foreground.acquire(abort));

    std::atomic_bool daemon_abort{false};
    bool acquired = true;
    std::chrono::milliseconds waited{0};
    std::thread daemon([&]() {
        SyncArbiter background(SyncRole::Background);
        waited = time_acquire(background, daemon_abort, acquired);
    });
    std::this_thread::sleep_for(400ms);
    daemon_abort = true;
    daemon.join();

    EXPECT_FALSE(acquired) << "acquire() muss bei Abbruch false liefern.";
    EXPECT_LT(waited.count(), 2000);
    foreground.release();
}

// acquire() is called once per pass, so the second call of a holder must not
// wait for itself.
TEST(SyncArbiterTests, RepeatedAcquireOfTheSameHolderReturnsAtOnce)
{
    std::atomic_bool abort{false};
    SyncArbiter foreground(SyncRole::Foreground);
    ASSERT_TRUE(foreground.acquire(abort));
    bool acquired = false;
    EXPECT_LT(time_acquire(foreground, abort, acquired).count(), 100);
    EXPECT_TRUE(acquired);
    foreground.release();
}
