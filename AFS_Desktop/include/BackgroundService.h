#pragma once

#include <string>

// Controls the backgroundsync daemon, which synchronises with the same
// configuration while the GUI is not running. The Windows service key and the
// systemd unit deliberately share this name, so one identifier addresses both.
namespace BackgroundService {

// Unknown covers the cases where the state cannot be determined; the caller
// then offers both commands instead of locking the user out on a query problem.
enum class State { NotInstalled, Stopped, Running, Unknown };

// Everything but a stopped service counts as running, so that the transitional
// states do not offer a start that the service control would reject anyway.
State state();

// An empty result means success, otherwise it holds the message for the user.
std::wstring start();
std::wstring stop();

}
