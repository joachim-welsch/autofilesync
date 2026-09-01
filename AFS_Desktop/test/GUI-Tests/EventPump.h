#pragma once

#include <functional>

namespace guitest {

// wxUIActionSimulator hands its events to the X server, so nothing happens
// until the application's event loop gets a chance to read them back.
void pump(int milliseconds = 50);
bool waitUntil(const std::function<bool()>& ready, int timeoutMilliseconds = 5000);

}
