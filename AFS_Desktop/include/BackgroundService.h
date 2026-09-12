#pragma once

#include <string>

namespace BackgroundService {  // [BackgroundService.md#BackgroundService]

enum class State { NotInstalled, Stopped, Running, Unknown };  // [BackgroundService.md#State]

State state();  // [BackgroundService.md#state]

std::wstring start();  // [BackgroundService.md#command-result]
std::wstring stop();

}
