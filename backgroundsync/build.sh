#!/bin/bash
set -euo pipefail

# Parse command line arguments
BUILD_RELEASE=false
BUILD_DEBUG=false

while getopts "rd" opt; do
  case $opt in
    r)
      BUILD_RELEASE=true
      ;;
    d)
      BUILD_DEBUG=true
      ;;
    \?)
      echo "Usage: $0 [-r] [-d]"
      echo "  -r  Build Release version"
      echo "  -d  Build Debug version"
      echo "  No options: Build both versions"
      exit 1
      ;;
  esac
done

# If no options specified, build both
if [ "$BUILD_RELEASE" = false ] && [ "$BUILD_DEBUG" = false ]; then
  BUILD_RELEASE=true
  BUILD_DEBUG=true
fi

# Build Release version
if [ "$BUILD_RELEASE" = true ]; then
  echo "Building Release version..."
  mkdir -p build/release
  cd build/release
  cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ../..
  cmake --build . --config Release --target backgroundsync
  cd ../..
fi

# Build Debug version
if [ "$BUILD_DEBUG" = true ]; then
  echo "Building Debug version..."
  mkdir -p build/debug
  cd build/debug
  cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ../..
  cmake --build . --config Debug --target backgroundsync
  cd ../..
fi
