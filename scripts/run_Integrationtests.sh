#!/bin/bash
# Builds and runs the integration tests of AFS_Core under Linux and macOS.
# Arguments are passed through to the test binary, e.g.
#   scripts/run_Integrationtests.sh --gtest_filter='*Sync*'
set -e
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DAFS_BUILD_TESTS=ON
cmake --build build --target Integrationtests -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
exec ./build/Integrationtests "$@"
