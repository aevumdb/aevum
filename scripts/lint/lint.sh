#!/bin/bash

##
# lint.sh
#
# This script runs clang-tidy static analysis and code quality checks on AevumDB.
# It analyzes C++ source code for potential bugs, modernization opportunities,
# and style violations, enforcing project code quality standards.
#
# Usage:
#   ./lint.sh                     # Check all C++ files
#   ./lint.sh src/aevum/db/       # Check specific directory
#   ./lint.sh src/aevum/main.cpp  # Check specific file
#   ./lint.sh --fix               # Auto-fix issues where possible
##

set -e

# Define the root directory of the project based on script location
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." &> /dev/null && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# Check if build directory and compile_commands.json exist
if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
    echo "Error: compile_commands.json not found in $BUILD_DIR"
    echo ""
    echo "Please build the project first:"
    echo "  ./scripts/build.sh"
    exit 1
fi

# Check if clang-tidy is installed
if ! command -v clang-tidy &> /dev/null; then
    echo "Error: clang-tidy not found"
    echo ""
    echo "Install clang-tidy:"
    echo "  Ubuntu/Debian: sudo apt install clang-tools"
    echo "  Fedora/RHEL: sudo dnf install clang-tools-extra"
    exit 1
fi

# Parse command-line arguments
TIDY_ARGS=""
TARGET_PATH="${PROJECT_ROOT}/src/aevum/"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --fix)
            # Enable automatic fixes
            TIDY_ARGS="${TIDY_ARGS} --fix"
            ;;
        --checks)
            # Custom checks argument
            shift
            if [[ -n "$1" ]]; then
                TIDY_ARGS="${TIDY_ARGS} --checks=$1"
            fi
            ;;
        -*)
            # Pass other flags directly to clang-tidy
            TIDY_ARGS="${TIDY_ARGS} $1"
            ;;
        *)
            # Assume it's a path
            if [[ -d "$PROJECT_ROOT/$1" || -f "$PROJECT_ROOT/$1" ]]; then
                TARGET_PATH="$PROJECT_ROOT/$1"
            else
                echo "Warning: Path '$1' not found, using default"
            fi
            ;;
    esac
    shift
done

echo "Running clang-tidy code analysis..."
echo "Target: $TARGET_PATH"
echo ""

# Run clang-tidy on C++ files
cd "$BUILD_DIR"
clang-tidy -p=. $TIDY_ARGS --header-filter="^$PROJECT_ROOT/src/.*" "$TARGET_PATH"/**/*.cpp "$TARGET_PATH"/**/*.hpp 2>/dev/null || true

echo ""
echo "Code analysis complete."
