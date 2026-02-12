#!/bin/bash
# ------------------------------------------------------------------------------
# AEVUMDB | Automated Kernel Build System
# ------------------------------------------------------------------------------
# Usage: ./build.sh
# ------------------------------------------------------------------------------
set -e

# --- Configuration & Colors ---
BOLD='\033[1m'
RED='\033[31m'
GREEN='\033[32m'
YELLOW='\033[33m'
BLUE='\033[34m'
CYAN='\033[36m'
GRAY='\033[90m'
RESET='\033[0m'

START_TIME=$(date +%s)

# --- Helper Functions ---
log_info() { echo -e "${BLUE}ℹ${RESET}  $1"; }
log_task() { echo -e "${GRAY}→${RESET}  $1"; }
log_success() { echo -e "${GREEN}✔${RESET}  $1"; }
log_error() { echo -e "${RED}✖  ERROR: $1${RESET}"; exit 1; }

# --- 1. Header & Environment Check ---
clear
echo -e "${CYAN}${BOLD}"
cat << "EOF"
    _                      ___  ___ 
   /_\  _____ ___  _ _ __ |   \| _ )
  / _ \/ -_) V / || | '  \| |) | _ \
 /_/ \_\___|\_/ \_,_|_|_|_|___/|___/

     Database Build System v1.0.1
EOF
echo -e "${RESET}"

# Verify toolchain availability
command -v cmake >/dev/null 2>&1 || log_error "CMake is not installed."
command -v cargo >/dev/null 2>&1 || log_error "Rust/Cargo is not installed."
command -v git >/dev/null 2>&1 || log_error "Git is not installed."
command -v clang-format >/dev/null 2>&1 || log_info "Clang-Format not found (Auto-formatting disabled)."

# Detect CPU Cores for parallel build
if [[ "$OSTYPE" == "darwin"* ]]; then
    CORES=$(sysctl -n hw.ncpu)
else
    CORES=$(nproc)
fi

log_info "Detected ${CORES} CPU cores. Parallel build enabled."

# --- 2. Dependency Resolution (Git Submodules) ---
echo -e "\n${BOLD}Dependency Resolution${RESET}"

if [ -d ".git" ]; then
    log_task "Synchronizing Git submodules..."
    git submodule update --init --recursive
    log_success "Submodules verified."
else
    log_info "Not a git repository, skipping submodule check."
fi

# --- 3. Build Process ---
echo -e "\n${BOLD}Building AevumDB Subsystems${RESET}"

# Format Rust code before building to ensure consistency
if [ -d "aevum_logic" ]; then
    log_task "Formatting Aevum Logic Engine (Rust)..."
    (cd aevum_logic && cargo fmt)
fi

# Prepare Build Directory
mkdir -p build
cd build

log_task "Configuring CMake (Release Mode)..."
# Using Release mode for maximum performance optimization
cmake -DCMAKE_BUILD_TYPE=Release .. > /dev/null

log_task "Compiling Kernel (C++ & Rust Hybrid)..."
# Execute Make. This triggers:
# 1. auto_format (C++)
# 2. rust_build (Cargo Release)
# 3. aevum_server (Linking)
# 4. aevum_tests (Test Suite)
make -j"${CORES}" --no-print-directory

cd ..

# --- 4. Summary & Timer ---
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

echo -e "\n${GREEN}${BOLD}BUILD SUCCESSFUL${RESET} in ${DURATION}s"
echo -e "────────────────────────────────────────────────"
echo -e "  ${BOLD}Artifacts:${RESET}"
echo -e "  • Server Kernel : ${CYAN}./build/aevum_server${RESET}"
echo -e "  • Test Runner   : ${CYAN}./build/tests/aevum_tests${RESET}"
echo -e ""
echo -e "  ${BOLD}Subsystems Loaded:${RESET}"
echo -e "  ${GRAY}Infra, FFI, Storage, Network, Rust-Core${RESET}"
echo -e ""
echo -e "  ${BOLD}Quick Start:${RESET}"
echo -e "  $ ./build/aevum_server ./aevum_data 5555"
echo -e "────────────────────────────────────────────────"
