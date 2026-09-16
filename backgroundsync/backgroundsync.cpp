#include <string>
#include <chrono>
#include <thread>
#include "Logging.h"
#include "Translation.h"
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdlib>
#include <syslog.h>
#include "MyDaemon.h"
#include <csignal>

std::unique_ptr<MyDaemon> myDaemon;
volatile std::sig_atomic_t stopRequested = 0;

void signalHandler(int /*signal*/) {
    stopRequested = 1;
}

int main(int argc, char *argv[])
{
    AFS_Core::translation::init_locale("de_DE.UTF-8");  // a unit file usually names no language at all
    AFS_Core::translation::init(AFS_Core::translation::locale_dir());  // before the first message; installed to /usr/sbin there is no locale dir beside it, so gettext searches the system path
    myDaemon = std::make_unique<MyDaemon>();
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    myDaemon->start();
    syslog(LOG_INFO, "backgroundsys läuft...");
    AFS_Core::log_info(L"backgroundsys gestartet", L"");
    while (!stopRequested) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    myDaemon->stop();
    return 0;
}

