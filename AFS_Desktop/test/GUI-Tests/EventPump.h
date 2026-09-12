#pragma once

#include <functional>

namespace guitest {

void pump(int milliseconds = 50);  // [EventPump.md#pump]
bool waitUntil(const std::function<bool()>& ready, int timeoutMilliseconds = 5000);

}
