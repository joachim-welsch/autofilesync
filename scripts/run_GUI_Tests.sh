#!/bin/bash
# Baut und startet die GUI-Tests von AFS_Desktop unter Linux und macOS -
# Gegenstueck zu run_GUI_Tests.ps1. Argumente werden an das Testprogramm
# durchgereicht, z.B.
#   scripts/run_GUI_Tests.sh --gtest_filter='*Papierkorb*'
set -e
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DAFS_BUILD_TESTS=ON
cmake --build build --target GuiTests -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
exec ./AFS_Desktop/test/GUI-Tests/run_gui_tests.sh build/GuiTests "$@"
