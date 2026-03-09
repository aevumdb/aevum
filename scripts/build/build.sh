#!/bin/bash

##
# build.sh
#
# This script orchestrates the complete build process for AevumDB.
# It handles CMake configuration, compilation of all targets (daemon, shell,
# and tests), and provides detailed feedback on the build status.
#
# Usage:
#   ./build.sh                    # Build with default settings
#   ./build.sh clean              # Clean build directory
#   ./build.sh [build-dir]        # Build in custom directory
#   ./build.sh -j N               # Build with N parallel jobs
##

set -e

# Define the root directory of the project based on script location
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." &> /dev/null && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# Default number of parallel jobs (use all available cores)
JOBS=$(nproc 2>/dev/null || echo 4)

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        clean)
            # Store clean action, but don't exit yet so BUILD_DIR can be set
            DO_CLEAN=true
            ;;
        -j)
            shift
            if [[ "$1" =~ ^[0-9]+$ ]]; then
                JOBS="$1"
            else
                echo "Error: -j requires a positive integer argument. Received: '$1'"
                exit 1
            fi
            ;;
        *)
            # Treat other non-flag arguments as custom build directory
            if [[ "$1" != -* ]]; then
                # If relative path, make it absolute relative to current directory
                if [[ "$1" = /* ]]; then
                    BUILD_DIR="$1"
                else
                    BUILD_DIR="$(pwd)/$1"
                fi
            else
                echo "Unknown option: $1"
                echo "Usage: ./build.sh [clean] [-j JOBS] [build-dir]"
                exit 1
            fi
            ;;
    esac
    shift
done

# Execute clean action if requested
if [ "$DO_CLEAN" = true ]; then
    echo "Cleaning build directory: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
    echo "Build directory cleaned."
    exit 0
fi

# Create the build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory: $BUILD_DIR"
    mkdir -p "$BUILD_DIR"
fi

# Initialize git submodules
echo "Initializing git submodules..."
cd "$PROJECT_ROOT"
git submodule update --init --recursive

# Change to the build directory
cd "$BUILD_DIR"

# Run CMake configuration
echo ""
echo "Configuring AevumDB with CMake..."
echo ""
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$PROJECT_ROOT"

# Build the project
echo ""
echo "Building AevumDB (using $JOBS parallel jobs)..."
echo ""
make -j"$JOBS"

# Display build summary
echo ""
echo "Build Complete!"
echo ""
echo "Daemon:  $BUILD_DIR/bin/aevumdb"
echo "Shell:   $BUILD_DIR/bin/aevumsh"
echo ""
echo "To run the daemon:"
echo "  $BUILD_DIR/bin/aevumdb"
echo ""
echo "To run the shell:"
echo "  $BUILD_DIR/bin/aevumsh"
echo ""
