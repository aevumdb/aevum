# Scripts Documentation

AevumDB includes utility scripts for building, formatting code, and development tasks. All scripts are located in the `/scripts/` directory.

## Quick Reference

```bash
# Build the project (default: Release mode)
./scripts/build.sh

# Clean and rebuild
./scripts/build.sh rebuild

# Format C++ code
./scripts/format.sh cpp

# Format Rust code
./scripts/format.sh rust

# Show help
./scripts/build.sh --help
```

## Build Scripts

### scripts/build.sh

Main build orchestrator script. Handles CMake configuration, compilation, and output organization.

**Location**: `/scripts/build.sh`

**Usage**:
```bash
./scripts/build.sh [COMMAND] [OPTIONS]
```

**Commands**:
- `(default)` - Build with CMake
- `clean` - Remove build artifacts
- `rebuild` - Clean + build
- `verbose` - Build with verbose output
- `help` - Show this message

**Options**:
- `-j N` - Build with `N` parallel jobs (e.g., `-j 4`)

**What it does**:
1. Creates `build/` directory if not present
2. Runs `cmake ..` to configure
3. Runs `make -j N` (default uses all available cores)
4. Outputs binaries to `build/bin/`

**Example**:
```bash
# Standard build using all cores
./scripts/build.sh

# Build with specifically 4 parallel jobs
./scripts/build.sh -j 4

# Verbose build to see compiler output
./scripts/build.sh verbose
```

### scripts/build/build.sh

Core build logic. Called by `scripts/build.sh`.

**Location**: `/scripts/build/build.sh`

**What it does**:
- Detects build type (Debug/Release)
- Creates build directory
- Runs CMake with appropriate flags
- Executes make with parallel jobs
- Verifies binary creation

**Configuration**:
- `CMAKE_BUILD_TYPE`: Release (optimized) or Debug
- Parallel jobs: Auto-detected (`$(nproc)`)
- Output: `build/bin/aevumdb` and `build/bin/aevumsh`

**Usually called automatically** - Use `./scripts/build.sh` instead.

## Formatting Scripts

### scripts/format.sh

Code formatting orchestrator. Formats C++, Rust, or all code.

**Location**: `/scripts/format.sh`

**Usage**:
```bash
./scripts/format.sh [LANGUAGE]
```

**Languages**:
- `cpp` - Format C++ code only (clang-format)
- `rust` - Format Rust code only (rustfmt)
- `(default)` - Format both C++ and Rust

**What it does**:
1. Finds all source files (`*.cpp`, `*.hpp`, `*.rs`)
2. Applies language-specific formatter
3. Modifies files in-place
4. Reports changes

**Example**:
```bash
# Format everything
./scripts/format.sh

# Format only C++
./scripts/format.sh cpp

# Format only Rust
./scripts/format.sh rust
```

### scripts/format/format.sh

Formatting orchestrator (called by `scripts/format.sh`).

**Location**: `/scripts/format/format.sh`

**Dispatches to**:
- `scripts/format/format-cpp.sh` - C++ formatting
- `scripts/format/format-rust.sh` - Rust formatting

### scripts/format/format-cpp.sh

C++ code formatting using `clang-format`.

**Location**: `/scripts/format/format-cpp.sh`

**What it does**:
1. Finds all `.cpp` and `.hpp` files in `src/`
2. Applies clang-format with configured style
3. Configuration from `.clang-format`

**Configuration**:
- Style file: `.clang-format` (project root)
- Format: LLVM-based with customizations
- Indent: 4 spaces
- Line length: ~100 characters

**Requirements**:
```bash
# Install clang-format
# Ubuntu/Debian
sudo apt install clang-format

# macOS
brew install clang-format

# Fedora
sudo dnf install clang-tools-extra
```

**Usage**:
```bash
./scripts/format/format-cpp.sh
```

### scripts/format/format-rust.sh

Rust code formatting using `rustfmt`.

**Location**: `/scripts/format/format-rust.sh`

**What it does**:
1. Finds all `.rs` files in `src/aevum/ffi/src/`
2. Applies rustfmt with project configuration
3. Configuration from `src/aevum/ffi/rustfmt.toml`

**Configuration**:
- Configuration file: `src/aevum/ffi/rustfmt.toml`
- Follow Rust edition conventions
- Automatic formatting per Rust standards

**Requirements**:
```bash
# rustfmt comes with Rust toolchain
# Install Rust if not present
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# Update rustfmt
rustup update
```

**Usage**:
```bash
./scripts/format/format-rust.sh
```

## Directory Structure

```
scripts/
├── build.sh                 # Main build script (user-facing)
├── format.sh                # Main format script (user-facing)
│
├── build/
│   └── build.sh            # Build implementation
│
└── format/
    ├── format.sh           # Format orchestrator
    ├── format-cpp.sh       # C++ formatting
    └── format-rust.sh      # Rust formatting
```

## Common Tasks

### Build for Development

```bash
# Debug build with symbols
mkdir -p build_dev
cd build_dev
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Build for Production

```bash
# Optimized release build
./scripts/build.sh rebuild
# Creates: build/bin/aevumdb (optimized)
```

### Format Before Commit

```bash
# Format all code
./scripts/format.sh

# Or just C++
./scripts/format.sh cpp

# Then commit
git add .
git commit -m "style: format code"
```

### Clean Rebuild

```bash
./scripts/build.sh rebuild
# Removes old build/ and starts fresh
```

### Build in Verbose Mode

```bash
./scripts/build.sh verbose
# Shows each compiler command
# Useful for debugging build issues
```

## Environment Variables

These can affect script behavior:

```bash
# Number of parallel make jobs
export MAKEFLAGS="-j4"

# CMake build type (Release/Debug/RelWithDebInfo/MinSizeRel)
# Usually set in scripts, but can override:
export CMAKE_BUILD_TYPE=Debug

# Compiler
export CC=gcc
export CXX=g++
# or: clang, clang++, etc.
```

## Troubleshooting

### Build fails: "CMake not found"

```bash
# Install CMake
sudo apt install cmake    # Ubuntu/Debian
brew install cmake        # macOS
sudo dnf install cmake    # Fedora
```

### Format fails: "clang-format not found"

```bash
# Install clang-format
sudo apt install clang-format    # Ubuntu/Debian
brew install clang-format        # macOS
sudo dnf install clang-tools-extra # Fedora
```

### Format fails: "rustfmt not found"

```bash
# rustfmt comes with Rust
# Install/update Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
rustup update
```

### Build slow with parallel jobs

```bash
# Reduce parallel jobs
make -j2    # Instead of make -j$(nproc)

# Or set environment variable
export MAKEFLAGS="-j2"
./scripts/build.sh
```

### Permission denied on script

```bash
# Make script executable
chmod +x ./scripts/build.sh
chmod +x ./scripts/format.sh
chmod -R +x ./scripts/build/
chmod -R +x ./scripts/format/

# Or add during clone
git clone https://github.com/aevumdb/aevum.git
cd aevum
chmod +x ./scripts/build.sh ./scripts/format.sh
```

## Git Integration

### Pre-commit Hook

Format code automatically before commit:

**`.git/hooks/pre-commit`**:
```bash
#!/bin/bash
./scripts/format.sh cpp
git add src/
```

Make executable:
```bash
chmod +x .git/hooks/pre-commit
```

## Advanced: Customizing Scripts

### Change Build Directory

Edit `scripts/build/build.sh`:
```bash
# Find line: BUILD_DIR="build"
# Change to: BUILD_DIR="custom_build"
```

### Use Different Compiler

```bash
export CC=clang
export CXX=clang++
./scripts/build.sh
```

### Change Output Location

```bash
mkdir -p /opt/aevumdb
cd /opt/aevumdb
cmake /path/to/aevumdb
make -j$(nproc)
```

## See Also

- [Building](BUILDING.md) - Detailed build instructions
- [Development](DEVELOPMENT.md) - Development workflow
- [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) - Directory layout
