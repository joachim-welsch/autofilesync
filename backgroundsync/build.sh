#!/bin/bash
# The daemon is built together with the GUI by the build script in the project root.
set -euo pipefail
SOURCE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
BUILD_RELEASE=false
BUILD_DEBUG=false
while getopts "rd" opt; do
  case $opt in
    r) BUILD_RELEASE=true ;;
    d) BUILD_DEBUG=true ;;
    \?)
      echo "Usage: $0 [-r] [-d]"
      echo "  -r  Build Release version"
      echo "  -d  Build Debug version"
      echo "  No options: Build both versions"
      exit 1
      ;;
  esac
done
if [ "$BUILD_RELEASE" = false ] && [ "$BUILD_DEBUG" = false ]; then
  BUILD_RELEASE=true
  BUILD_DEBUG=true
fi
[ "$BUILD_RELEASE" = true ] && "${SOURCE_DIR}/build.sh" -r
[ "$BUILD_DEBUG" = true ] && "${SOURCE_DIR}/build.sh" -d
exit 0
