#include <string>
#include <chrono>
#include <thread>
#include "Logging.h"
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
    myDaemon = std::make_unique<MyDaemon>();
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    myDaemon->start();
    syslog(LOG_INFO, "backgroundsys läuft...");
    LOG_INFO2(L"backgroundsys gestartet", L"");
    while (!stopRequested) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    myDaemon->stop();
    return 0;
}

