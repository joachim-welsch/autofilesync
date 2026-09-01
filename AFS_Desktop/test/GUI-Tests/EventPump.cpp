#include "EventPump.h"
#include <wx/app.h>
#include <wx/stopwatch.h>
#include <wx/utils.h>

namespace guitest {

void pump(int milliseconds)
{
    wxStopWatch watch;
    do {
        wxYield();
        wxMilliSleep(5);
    } while (watch.Time() < milliseconds);
    wxYield();
}

bool waitUntil(const std::function<bool()>& ready, int timeoutMilliseconds)
{
    wxStopWatch watch;
    while (!ready()) {
        if (watch.Time() > timeoutMilliseconds)
            return false;
        pump(10);
    }
    return true;
}

}
