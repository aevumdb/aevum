# Development Guide

Set up your development environment and start contributing to AevumDB.

## Development Setup

### Clone Repository

```bash
git clone https://github.com/aevumdb/aevum.git
cd aevum
```

### Install Dependencies

**Recommendation**: Use `./scripts/build.sh` as it handles these dependencies automatically for you.

**Ubuntu 20.04+**
```bash
sudo apt update
sudo apt install -y build-essential cmake g++ git python3 pkg-config gdb lldb valgrind clang-format
```

**Fedora 33+**
```bash
sudo dnf install -y cmake gcc-c++ python3 pkgconfig gdb llvm-tools valgrind clang-tools-extra
```

### Build for Development

```bash
# Create debug build
mkdir -p build_dev
cd build_dev
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

Debug binaries at:
- `build_dev/bin/aevumdb` - Server daemon
- `build_dev/bin/aevumsh` - Shell client

## Project Structure

```
aevumdb/
├── src/aevum/
│   ├── main.cpp              # Daemon entry point
│   ├── bson/                 # Document serialization
│   │   ├── doc/              # Document model (Document, Builder, Iterator, Json)
│   │   └── json/             # JSON utilities
│   ├── client/               # Client-side code
│   │   ├── aevum_client.hpp  # High-level C++ API
│   │   └── net/              # Network communication
│   │       ├── connection.hpp # TCP client connection
│   │       └── server.hpp     # TCP server listener
│   ├── db/                   # Database engine
│   │   ├── core/
│   │   │   ├── core.hpp      # Main orchestrator (Core class)
│   │   │   └── core.cpp
│   │   ├── auth/             # Authentication & authorization
│   │   ├── storage/          # WiredTiger integration
│   │   ├── schema/           # Schema validation
│   │   └── index/            # Indexing system
│   ├── shell/                # Interactive shell
│   │   ├── main.cpp          # Shell entry point
│   │   ├── repl/             # Read-Eval-Print Loop
│   │   │   ├── repl.hpp
│   │   │   └── repl.cpp
│   │   └── parser/           # Command parsing
│   │       ├── command_parser.hpp
│   │       └── command_parser.cpp
│   └── util/                 # Utilities
│       ├── log/              # Logging
│       ├── memory/           # Memory management
│       ├── string/           # String utilities
│       ├── status.hpp        # Status/error codes
│       └── ...
├── third_party/
│   ├── wiredtiger/           # Storage engine
│   ├── mongo-c-driver/       # BSON library
│   └── simdjson/             # Fast JSON parsing
├── CMakeLists.txt            # Build configuration
└── scripts/                  # Build and utility scripts
    ├── build.sh              # Main build script
    ├── format.sh             # Code formatting
    └── ...
```

## Key Classes

### db/core/core.hpp - The Heart
**Responsibility**: Orchestrates all database subsystems
**Main methods**:
- `insert(collection, document)` - Add document
- `find(collection, query)` - Query documents
- `update(collection, query, update)` - Modify documents
- `remove(collection, query)` - Delete documents
- `count(collection, query)` - Count matching docs
- `upsert(collection, query, document)` - Update or insert

### client/aevum_client.hpp - Client API
**Responsibility**: High-level C++ interface for clients
**Main methods**:
- `connect()` - Connect to server
- `disconnect()` - Close connection
- `insert(collection, json)` - Send insert command
- `find(collection, query_json)` - Send find command
- `update(collection, query_json, update_json)` - Send update
- `count(collection, query_json)` - Send count

### bson/doc/document.hpp - Document Model
**Responsibility**: BSON document representation
- In-memory representation
- Builder for constructing documents
- Iterator for traversing
- JSON conversion

### storage/wiredtiger_store.hpp - Persistence
**Responsibility**: Physical data storage
- Connect to WiredTiger
- Collections as tables
- ACID transactions
- Compression, caching

## Making Changes

### Adding a Database Operation

Example: Add a new `exists()` operation

1. **Add to Core class** (`src/aevum/db/core/core.hpp`):
```cpp
aevum::util::Status exists(std::string_view collection, std::string_view query_json);
```

2. **Implement** (`src/aevum/db/core/core.cpp`):
```cpp
Status Core::exists(std::string_view collection, std::string_view query_json) {
    // Implementation
}
```

3. **Add to AevumClient** (`src/aevum/client/aevum_client.hpp`):
```cpp
[[nodiscard]] std::string exists(std::string_view collection, std::string_view query_json);
```

4. **Add parser support** (`src/aevum/shell/parser/command_parser.cpp`):
```cpp
} else if (operation == "exists") {
    response = client.exists(collection, args_str);
}
```

5. **Update help** (`src/aevum/shell/repl/repl.cpp`):
```cpp
"  db.<coll>.exists({ <query> })              - Check if documents exist.\n"
```

### Code Style

**C++ Conventions**
- **Naming**: camelCase for variables/functions, PascalCase for classes
- **Indent**: 4 spaces (not tabs)
- **Line length**: ~100 characters preferred
- **Comments**: Doxygen style `/** ... */`
- **Headers**: Include guards, minimal includes

**Example**:
```cpp
// Doxygen comment
/**
 * @brief Brief description.
 * @param param1 Parameter description.
 * @return Return description.
 */
Status myFunction(std::string_view param1) {
    // Implementation
    return status;
}
```

**Auto-format code**:
```bash
./scripts/format.sh cpp
```

**Code analysis and linting**:
```bash
./scripts/lint.sh
```

## Building & Testing

### Build Debug Version
```bash
mkdir -p build_dev
cd build_dev
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Run Daemon
```bash
./build_dev/bin/aevumdb
```

Should output:
```
[INFO] Initializing AevumDB core...
[INFO] Starting AevumDB server on 127.0.0.1:55001
```

### Connect with Shell
```bash
./build_dev/bin/aevumsh
```

### Test Your Changes
```bash
> db.test.insert({value: "hello"})
> db.test.find({})
> db.test.delete({})
> exit
```

## Debugging

### With GDB (Linux)

```bash
gdb ./build_dev/bin/aevumdb

# Inside GDB
(gdb) run
(gdb) break main
(gdb) continue
(gdb) print variable_name
(gdb) next
(gdb) quit
```

### Debug Output

Add logging to code:
```cpp
#include "aevum/util/log/logger.hpp"

// Then use
AEVUM_LOG(INFO, "Message: " << value);
AEVUM_LOG(ERROR, "Error occurred");
AEVUM_LOG(DEBUG, "Debug info");
```

### Memory Checking (Valgrind)

```bash
valgrind --leak-check=full ./build_dev/bin/aevumdb
```

## Common Development Tasks

### Running Shell Commands

Start daemon:
```bash
./build_dev/bin/aevumdb
```

In another terminal:
```bash
./build_dev/bin/aevumsh

# Try commands
> db.users.insert({name: "Alice"})
> db.users.find({})
> help
> exit
```

### Checking Code Format

```bash
./scripts/format.sh cpp --check
```

### Cleaning Build

```bash
rm -rf build_dev
```

### Rebuilding Everything

```bash
rm -rf build_dev
mkdir -p build_dev
cd build_dev
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

## Git Workflow

### Clone and setup
```bash
git clone https://github.com/aevumdb/aevum.git
cd aevum
git config user.email "your@email.com"
git config user.name "Your Name"
```

### Create feature branch
```bash
git checkout -b feature/my-feature
```

### Commit changes
```bash
git add .
git commit -m "feat: add awesome feature"
```

### Push branch
```bash
git push origin feature/my-feature
```

### Create pull request
Open PR on GitHub explaining your changes.

## Code Review Checklist

Before submitting PR:
- [ ] Code compiles without warnings
- [ ] Code is formatted: `./scripts/format.sh cpp`
- [ ] Changes follow style guide
- [ ] Manual testing done
- [ ] Documentation updated (if needed)
- [ ] No debug output in production code

## Performance Tips

### Profiling
```bash
# Build with profiling support
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..

# Run with Linux perf
perf record -g ./build_dev/bin/aevumdb
perf report
```

### Optimization
- Use `-march=native` for CPU-specific optimizations
- Enable LTO: `cmake -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=True ..`
- Profile before optimizing

## Testing

### FFI Unit Tests

Run Rust FFI module tests:

```bash
# Run all FFI tests
./scripts/test.sh

# Run specific test with output
./scripts/test.sh -- test_name --nocapture

# Run tests sequentially
./scripts/test.sh -- --single-threaded
```

### Manual Testing Workflow

```bash
# Terminal 1: Start daemon
./build_dev/bin/aevumdb

# Terminal 2: Connect shell
./build_dev/bin/aevumsh

# Try operations
> db.test.insert({x: 1})
> db.test.find({})
> db.test.update({x: 1}, {y: 2})
> db.test.count({})
> db.test.delete({x: 1})
> exit
```

## Resources

### Understanding the Code
1. Start with `src/aevum/main.cpp` - Server initialization
2. Look at `src/aevum/shell/main.cpp` - Client initialization
3. Study `src/aevum/db/core/core.hpp` - Main logic
4. Explore `src/aevum/client/aevum_client.hpp` - Client API

### External Libraries
- [WiredTiger Docs](http://source.wiredtiger.com/develop/)
- [BSON Spec](https://bsonspec.org/)
- [simdjson Docs](https://simdjson.org/)

## See Also

- [Building](BUILDING.md) - Detailed build instructions
- [Architecture](ARCHITECTURE.md) - System design
- [Shell Reference](SHELL_REFERENCE.md) - Commands  
- [Troubleshooting](TROUBLESHOOTING.md) - Common issues
- [Project Structure](PROJECT_STRUCTURE.md) - Directory layout
- [Scripts](SCRIPTS.md) - Build automation
