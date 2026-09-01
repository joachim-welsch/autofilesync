#include "MyDaemon.h"
#include <ctime>
#include <Synchronize.h>
#include <config.h>
#include "Logging.h"

MyDaemon::MyDaemon()
    : running_(false) {
}

MyDaemon::~MyDaemon() {
    stop();
}

void MyDaemon::start() {
    if (running_.load()) {
        return;
    }
    running_.store(true);
    Config::instance().read(L"/home/welschj/backgroundsync.json");
    LOG_INFO(L"Daemon is running...");
    workerThread_ = std::thread(&MyDaemon::run, this);
}

void MyDaemon::stop() {
    if (!running_.load()) {
        return;
    }
    running_.store(false);
    if (workerThread_.joinable())
        workerThread_.join();
}

void MyDaemon::run() {
    Synchronize::create(SyncType::Continuous);
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    Synchronize::destroy();
}

// void MyDaemon::logMessage(const std::string& message) {
//     auto now = std::chrono::system_clock::now();
//     auto now_c = std::chrono::system_clock::to_time_t(now);
//     logFile_ << std::put_time(std::localtime(&now_c), "%F %T") << " - " << message << std::endl;
// }
