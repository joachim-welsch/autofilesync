#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <csignal>

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
};
