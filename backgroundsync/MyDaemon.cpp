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
    const std::wstring path_name = Config::get_root_pathname();  // the GUI's file, so both sync the same items
    Config::instance().pathname(path_name);
    Config::instance().read(path_name);
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
    Synchronize::create(SyncType::Continuous, nullptr, SyncRole::Background);
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    Synchronize::destroy();
}
