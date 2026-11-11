#!/bin/bash
# Builds and runs every test of the phone functions under Linux and macOS:
#   integration tests  DeviceLinkTest (pairing, hello, passes against FakePhone)
#   GUI tests          PairDeviceTest, FirewallHint, UsbPhoneSyncTest
# UsbPhoneSyncTest needs a real Android phone on USB (debug build of the app,
# USB debugging, "access to all files", unlocked); without one it is skipped.
# Arguments are passed through to both test binaries, e.g.
#   scripts/run_Phone_Tests.sh --gtest_repeat=3
# --install-app first builds and installs the debug app on the connected phone.
set -e
cd "$(dirname "$0")/.."
if [ "$1" = "--install-app" ]; then
    shift
    (cd AFS_Android && ./gradlew installDebug)
fi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DAFS_BUILD_TESTS=ON
cmake --build build --target Integrationtests GuiTests -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
status=0
./build/Integrationtests --gtest_filter='DeviceLinkTest.*' "$@" || status=1
./AFS_Desktop/test/GUI-Tests/run_gui_tests.sh build/GuiTests \
    --gtest_filter='PairDeviceTest.*:FirewallHint.*:UsbPhoneSyncTest.*' "$@" || status=1
[ $status -eq 0 ] && echo "Alle Phone-Tests bestanden." || echo "Phone-Tests fehlgeschlagen." >&2
exit $status
