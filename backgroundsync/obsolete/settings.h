#pragma once
#include <stdint.h>

class Settings {
    bool consoleMode_       = false;
    bool startMinimized_    = false;
    bool autoStart_         = false;
    bool startSyncAtStart_  = false;
    uint16_t syncCycle_     = 1;
public:
    void setConsoleMode() { consoleMode_=true; }
    bool consoleMode() const { return consoleMode_; }
    bool startMinimized() const { return startMinimized_; }
    void startMinimized(bool sm) { startMinimized_=sm; }
    bool autoStart() const { return autoStart_; }
    void autoStart(bool as) { autoStart_=as; }
    bool startSyncAtStart() const { return startSyncAtStart_; }
    void startSyncAtStart(bool as) { startSyncAtStart_=as; }
    void syncCycle(uint16_t p) { syncCycle_=p; }
    uint16_t syncCycle() const { return syncCycle_; }
    void reset();
};
