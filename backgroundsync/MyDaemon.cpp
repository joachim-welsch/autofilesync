#include "MyDaemon.h"
#include <ctime>
#include <Synchronize.h>
#include <Config.h>
#include "Logging.h"

using namespace AFS_Core;

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
    log_info(L"Daemon is running...");
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
