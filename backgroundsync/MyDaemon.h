#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <csignal>
#include <LogCut.h>

class MyDaemon {
public:
    MyDaemon();
    ~MyDaemon();

    void start();
    void stop();

private:
    void run();

    std::atomic<bool> running_;
    std::thread workerThread_;
    AFS_Core::DailyLogCut cut_log_daily_;
};
