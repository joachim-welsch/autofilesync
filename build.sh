#!/bin/bash
# AutoFileSync Build Script

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default build type
BUILD_TYPE="Release"
CLEAN=false
STRIP=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        clean)
            CLEAN=true
            shift
            ;;
        strip)
            STRIP=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -d, --debug     Build Debug version (default: Release)"
            echo "  -r, --release   Build Release version"
            echo "  clean           Clean build directory before building"
            echo "  strip           Strip debug symbols from binary"
            echo "  -h, --help      Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0              # Build Release"
            echo "  $0 -d           # Build Debug"
            echo "  $0 clean -d     # Clean and build Debug"
            echo "  $0 -r strip     # Build Release and strip symbols"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

echo -e "${GREEN}=== AutoFileSync ${BUILD_TYPE} Build ===${NC}"

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Set build directory based on build type
if [ "$BUILD_TYPE" == "Debug" ]; then
    BUILD_DIR="${SCRIPT_DIR}/build/Debug"
else
    BUILD_DIR="${SCRIPT_DIR}/build/Release"
fi

# Clean build directory if requested
if [ "$CLEAN" == true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "${BUILD_DIR}"
    echo -e "${GREEN}Build directory cleaned.${NC}"
fi

# Create build directory
echo -e "${YELLOW}Creating build directory...${NC}"
mkdir -p "${BUILD_DIR}"

# Configure CMake
echo -e "${YELLOW}Configuring CMake...${NC}"
cmake -S "${SCRIPT_DIR}" \
      -B "${BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
      -DCMAKE_CXX_STANDARD=20

# Build
echo -e "${YELLOW}Building project...${NC}"
cmake --build "${BUILD_DIR}" \
      --config ${BUILD_TYPE} \
      --target autofilesync \
      -j "$(getconf _NPROCESSORS_ONLN)"

# macOS builds an app bundle instead of a plain executable
if [ "$(uname)" == "Darwin" ]; then
    APP="${BUILD_DIR}/autofilesync.app"
    EXECUTABLE="${APP}/Contents/MacOS/autofilesync"
    INSTALL_HINT="cp -R ${APP} /Applications/"
    RUN_HINT="open ${APP}"
else
    EXECUTABLE="${BUILD_DIR}/autofilesync"
    INSTALL_HINT="sudo cp ${EXECUTABLE} /usr/local/bin/"
    RUN_HINT="${EXECUTABLE}"
fi

# Check if build was successful
if [ -f "${EXECUTABLE}" ]; then
    echo -e "${GREEN}=== Build successful! ===${NC}"
    echo -e "Executable: ${EXECUTABLE}"

    # Show file size
    SIZE=$(du -h "${EXECUTABLE}" | cut -f1)
    echo -e "Size: ${SIZE}"
    echo -e "Build type: ${BUILD_TYPE}"

    # Strip symbols for smaller binary (optional)
    if [ "$STRIP" == true ]; then
        echo -e "${YELLOW}Stripping debug symbols...${NC}"
        strip "${EXECUTABLE}"
        SIZE_STRIPPED=$(du -h "${EXECUTABLE}" | cut -f1)
        echo -e "Size after strip: ${SIZE_STRIPPED}"
    fi

    echo -e "\n${GREEN}To install:${NC}"
    echo -e "  ${INSTALL_HINT}"
    echo -e "\n${GREEN}To run:${NC}"
    echo -e "  ${RUN_HINT}"
else
    echo -e "${RED}=== Build failed! ===${NC}"
    exit 1
fi
