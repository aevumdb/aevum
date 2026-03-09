# Building AevumDB

Complete guide to building AevumDB from source with various configuration options.

## Quick Build

```bash
chmod +x ./scripts/build.sh ./scripts/build/build.sh
./scripts/build.sh
```

Outputs to: `build/bin/aevumdb` and `build/bin/aevumsh`

**Note:** The build script automatically initializes git submodules. If you're cloning for the first time, you can also use:
```bash
git clone --recursive https://github.com/aevumdb/aevum.git
```

## Prerequisites

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake g++ git python3 pkg-config cargo
```

### Fedora/RHEL

```bash
sudo dnf install -y cmake gcc-c++ git python3 pkgconfig cargo
```

## Build Process

### Standard Build

```bash
# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build
make -j$(nproc)
```

Outputs:
- `bin/aevumdb` - Database daemon server
- `bin/aevumsh` - Interactive shell client

### Build Script

The provided build script handles everything:

```bash
./scripts/build.sh
```

**Options:**
```bash
./scripts/build.sh clean      # Clean build artifacts
./scripts/build.sh rebuild    # Clean + build
./scripts/build.sh verbose    # Build with verbose output
```

## Build Modes

### Debug Build (with symbols, no optimization)

```bash
mkdir -p build_debug
cd build_debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

Useful for:
- Debugging with GDB/LLDB
- Detailed error messages
- Development/testing

### Release Build (optimized, stripped)

```bash
mkdir -p build_release
cd build_release
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

Useful for:
- Production deployment
- Maximum performance
- Minimal binary size

### MinSizeRel (optimized, smallest)

```bash
mkdir -p build_minsize
cd build_minsize
cmake -DCMAKE_BUILD_TYPE=MinSizeRel ..
make -j$(nproc)
```

## Components Built

### Binaries

1. **aevumdb**
   - Main daemon server process
   - Location: `build/bin/aevumdb`
   - Listens on port 55001 by default

2. **aevumsh**
   - Interactive shell client
   - Location: `build/bin/aevumsh`
   - Connects to aevumdb daemon

### Libraries (Internal)

Built statically and linked into binaries:
- **libbson** - BSON document serialization (from mongo-c-driver)
- **simdjson** - Fast JSON parsing
- **WiredTiger** - Embedded storage engine

## Configuration

### CMake Variables

Override with: `cmake -D<VAR>=<VALUE> ..`

**Common variables:**

```bash
# Build type
-DCMAKE_BUILD_TYPE=Release|Debug|MinSizeRel

# Compiler version
-DCMAKE_CXX_STANDARD=17

# Install prefix
-DCMAKE_INSTALL_PREFIX=/usr/local

# Verbose output
-DCMAKE_VERBOSE_MAKEFILE=ON
```

**Example:**
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/aevumdb ..
make -j$(nproc)
```

## Parallel Compilation

Use all CPU cores:

```bash
make -j$(nproc)
```

Or specific count:
```bash
make -j4
```

## Cleaning Build

Remove build artifacts:

```bash
# Clean within build directory
make clean

# Full rebuild from scratch
rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Build Verification

After building, verify binaries exist:

```bash
ls -lh build/bin/aevumdb
ls -lh build/bin/aevumsh
```

Quick functionality test:

```bash
# Create temp data directory
mkdir -p /tmp/aevumdb_test

# Start daemon
./build/bin/aevumdb &
DAEMON_PID=$!

# Wait for startup
sleep 2

# Connect and test
echo "db.test.insert({name: \"test\"})" | ./build/bin/aevumsh

# Kill daemon
kill $DAEMON_PID
```

## Code Formatting and Analysis

Before committing code:

```bash
# Format all C++ code
./scripts/format.sh cpp

# Format all Rust code
./scripts/format.sh rust

# Format everything
./scripts/format.sh
```

Run code analysis and linting:

```bash
# Analyze all C++ code
./scripts/lint.sh

# Fix issues automatically where possible
./scripts/lint.sh --fix
```

Requirements:
- **C++**: clang-format (installed via build-essential/development tools)
- **C++ Analysis**: clang-tidy (installed via clang-tools)
- **Rust**: rustfmt (installed with Rust toolchain)

## Common Build Issues

### CMake not found

Ubuntu/Debian:
```bash
sudo apt install cmake
```

Fedora/RHEL:
```bash
sudo dnf install cmake
```

### C++ compiler missing

Ubuntu/Debian:
```bash
sudo apt install build-essential
```

Fedora/RHEL:
```bash
sudo dnf install gcc-c++
```

### WiredTiger build fails

Ubuntu/Debian:
```bash
sudo apt install python3 pkg-config
```

Fedora/RHEL:
```bash
sudo dnf install python3 pkgconfig
```

# Clean and rebuild
rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Out of memory during build

Limit parallel jobs:

```bash
make -j2
```

Or use system swap:

```bash
# Create 4GB swap file (Ubuntu)
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

### Permission denied on install

```bash
# Use prefix to install to user directory
mkdir -p ~/.local
cmake -DCMAKE_INSTALL_PREFIX=~/.local ..
make -j$(nproc)
make install
```

## Installation

### System-wide (requires sudo)

```bash
cd build
sudo make install
```

Default location: `/usr/local/bin/`

### User directory (no sudo needed)

```bash
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=~/.local ..
make -j$(nproc)
make install
```

Binaries at: `~/.local/bin/aevumdb` and `~/.local/bin/aevumsh`

Add to PATH if needed:
```bash
export PATH="$HOME/.local/bin:$PATH"
```

### Custom location

```bash
cmake -DCMAKE_INSTALL_PREFIX=/opt/aevumdb ..
make -j$(nproc)
sudo make install
```

## Performance Optimization

### Compiler Optimization Levels

Release mode uses `-O3` by default for maximum performance.

For maximum size optimization:
```bash
cmake -DCMAKE_BUILD_TYPE=MinSizeRel ..
```

### Link-Time Optimization (LTO)

Enable if supported:
```bash
cmake -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=True ..
```

### CPU-Specific Features

Enable SIMD optimizations:
```bash
cmake -DCMAKE_CXX_FLAGS="-march=native" ..
```

## Cross-Compilation

For other architectures/platforms:

```bash
# Example: ARM64 on Linux x86_64
cmake -DCMAKE_SYSTEM_NAME=Linux \
      -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
      -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
      -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ ..
```

## Troubleshooting Build

### Check CMake version
```bash
cmake --version
```
Must be 3.21 or newer.

### Check compiler version
```bash
g++ --version    # or clang++
```
Must support C++17.

### Verbose error output
```bash
make VERBOSE=1
```

### Check compiler path
```bash
which gcc g++ clang clang++
```

### Debug CMake
```bash
cmake --debug-output ..
```

## See Also

- [Getting Started](GETTING_STARTED.md) - Quick start guide
- [Development](DEVELOPMENT.md) - Development setup
- [Deployment](DEPLOYMENT.md) - Production deployment
- [Scripts](SCRIPTS.md) - Build script documentation
- [Troubleshooting](TROUBLESHOOTING.md) - Common issues
