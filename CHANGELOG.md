# Changelog

All notable changes to AevumDB are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.2] - 2026-03-15

### Added
- **Command History**: Integrated the `linenoise` library to provide persistent command history and advanced line editing in `aevumsh`.
- **Persistent History File**: User commands are now saved to `~/.aevum_history` across sessions.

### Optimized
- **Build System Refinement**: Refactored the build configuration to separate C (`linenoise`) and C++ (`aevum_core`) targets, resolving language-specific Precompiled Header (PCH) conflicts and improving build reliability.

### Updated
- `src/aevum/shell/repl/repl.cpp` - Replaced standard input handling with `linenoise` for improved UX.
- `CMakeLists.txt` - Integrated `linenoise` as a standalone static library.
- `Cargo.toml`, `CMakeLists.txt` - Bumped version to 1.2.2.

## [1.2.1] - 2026-03-14

### Updated
- `src/aevum/shell/repl/repl.cpp` - Refactored `print_help` to use a clean, professional style similar to `mongosh`.
- `src/aevum/shell/main.cpp` - Improved CLI usage message for the shell client.
- `docs/SHELL_REFERENCE.md`, `docs/GETTING_STARTED.md`, `docs/README.md` - Synchronized all user documentation with the new v1.2.1 shell output standards and system commands.
- `CMakeLists.txt`, `src/aevum/ffi/Cargo.toml` - Bumped version to 1.2.1.

## [1.2.0] - 2026-03-14

### Added
- **Persistent Vendor Strategy**: Optimized third-party library management by moving WiredTiger and libbson builds to `third_party/dist`, ensuring they persist across `rm -rf build` operations.
- **Enterprise-Grade Installation**: Introduced a new system-wide installation suite (`scripts/install.sh`) that automates the full development lifecycle (Format, Build, Lint, Test, and Deploy).
- **Systemd Daemon Support**: Official support for running AevumDB as a persistent background service with automatic restart and boot management.
- **Dynamic Configuration**: Daemon now supports reading operational parameters (dbPath, port, logging) from `/etc/aevum/aevumdb.conf` via the `--config` flag.
- **Centralized Deployment**: Standardized installation in `/opt/aevumdb` with global symbolic links in `/usr/local/bin` for seamless terminal access.

### Optimized
- **Advanced Compilation Pipeline**: Integrated **Ninja** as the primary build generator and **ccache** for significantly faster incremental builds.
- **Build Performance**: Implemented **Unity Builds** and **Precompiled Headers (PCH)**, reducing the total compilation time for internal C++ source code by up to 60%.
- **Core Engine Refactoring**: Consolidated database logic into a shared `aevum_core` library, eliminating redundant compilation cycles for the daemon and shell binaries.
- **Streamlined Outputs**: Cleaned up all shell script outputs and internal comments, removing decorative symbols for a more professional and modern terminal experience.

### Updated
- `docs/BUILDING.md`, `docs/DEPLOYMENT.md`, `docs/SCRIPTS.md`
- `src/aevum/ffi/Cargo.toml`
### Fixed
- Fixed `stoi` runtime exception during systemd startup by implementing a robust manual configuration parser for the daemon.
- Resolved target naming conflicts in `CMakeLists.txt` during FFI integration.
- Fixed WiredTiger build failures on **GCC 15** by suppressing strict array-bounds warnings and disabling mandatory `-Werror` for third-party code.

## [1.1.2] - 2026-03-13

### Added
- Support for the **Ninja** build system as the primary generator for faster builds.
- Integrated **ccache** support for near-instant rebuilds.
- Automatic installation of `ninja-build` and `ccache` in the build script.

### Updated
- `CMakeLists.txt` - Fixed double compilation of source files by introducing a shared `aevum_core` static library and cleaned up duplicated Rust FFI logic.
- `scripts/build/build.sh` - Enhanced to prioritize Ninja, enable ccache, and automate installation of new build tools.
- `docs/BUILDING.md` - Added documentation for Ninja, ccache, and build performance optimizations.
- `docs/SCRIPTS.md` - Documented enhanced build script features and automated tool detection.
- `src/aevum/ffi/Cargo.toml` - Bumped version to 1.1.2.

## [1.1.1] - 2026-03-12

### Added
- Automated dependency check and installation in `scripts/build/build.sh`.
- Support for automatic installation of C/C++ compilers, Rust, and CMake across Arch, Debian/Ubuntu, and Fedora.

### Updated
- `scripts/build/build.sh` - Integrated `check_and_install_deps` function for a smoother developer experience on fresh systems.
- `README.md` - Added note about automated dependency installation.
- `docs/BUILDING.md` - Updated with automated dependency information and simplified build instructions.
- `docs/SCRIPTS.md` - Documented new dependency detection logic in build scripts.
- `docs/GETTING_STARTED.md` - Added prerequisite automation notes.
- `docs/DEVELOPMENT.md` - Simplified dependency setup for new contributors.

## [1.1.0] - 2026-03-11

### Added
- `clear` command to the interactive shell for terminal screen management.
- Comprehensive documentation updates for shell built-in commands.

### Updated
- `src/aevum/shell/repl/repl.cpp` - Enhanced REPL logic and internal help output.
- `docs/SHELL_REFERENCE.md` - Expanded documentation for built-in shell operations.
- `docs/GETTING_STARTED.md` - Added usage examples for shell built-in commands.
- `docs/README.md` - Synchronized documentation metadata.

## [1.0.3] - 2026-03-10

### Added
- Code analysis and linting script suite
- `scripts/lint.sh` - Main lint orchestrator for clang-tidy analysis
- `scripts/lint/lint.sh` - Core implementation for C++ code analysis
- Support for auto-fixing code issues with `--fix` flag

### Fixed
- Fixed CMake configuration to export compile commands for clang-tidy support
- Updated build script to generate `compile_commands.json` needed by lint tools

### Updated
- Enhanced SCRIPTS.md documentation with lint script details
- Expanded CODEOWNERS with comprehensive path coverage
- Updated CONTRIBUTING.md with linting guidelines
- Updated PR template with linting checklist
- Focused documentation on Linux platform (Ubuntu, Fedora, Debian)

## [1.0.2] - 2026-03-09

### Added
- Test script suite for FFI module testing
- `scripts/test.sh` - Main test orchestrator for cargo test execution
- `scripts/test/test.sh` - Core test implementation for `src/aevum/ffi`
- FFI unit test documentation in development guide

### Updated
- Enhanced SCRIPTS.md documentation with test script details
- Updated DEVELOPMENT.md testing section with FFI test procedures
- Updated directory structure documentation to include test folder
- Added test script checklist to PR template
- Improved CONTRIBUTING.md with testing guidelines
- Expanded CODEOWNERS with comprehensive path coverage

## [1.0.1] - 2026-03-07

### Fixed
- Build script parallel jobs validation (fix `-j N` error)
- Fixed `.gitignore` accidentally ignoring build scripts
- Improved CI reliability by focusing on Linux builds
- Fixed Dependabot configuration errors
- Updated Security Audit workflow to use CodeQL v3

## [1.0.0] - 2026-03-07

### Added
- Initial release
- Core database engine with document storage
- Interactive shell with MongoDB-like interface
- BSON serialization support
- WiredTiger storage backend
- Index support (single field)
- Basic query operators ($gt, $lt, $eq, $ne)
- CRUD operations (insert, find, update, delete)
- Connection pooling
- Authentication framework
- Transaction support
- Data export/import functionality

### Documentation
- Complete user guide
- API reference
- Architecture overview
- Installation instructions
- Development setup guide
- Deployment guide
- Troubleshooting guide

## Version History

- `1.2.2` - Command history and advanced line editing (March 15, 2026)
- `1.2.1` - Shell help improvements and standardization (March 14, 2026)
- `1.2.0` - Persistent Vendor & System-wide Installation (March 14, 2026)
- `1.1.2` - Build system optimizations (ccache, Ninja, Core Lib) (March 13, 2026)
- `1.1.1` - Automated dependency installation (March 12, 2026)
- `1.1.0` - Shell improvements and 'clear' command (March 11, 2026)
- `1.0.3` - Code linting script suite (March 10, 2026)
- `1.0.2` - Test script and documentation updates (March 9, 2026)
- `1.0.1` - Build system and CI configuration fixes (March 7, 2026)
- `1.0.0` - Initial public release (March 7, 2026)

## Upgrading

### From 0.x to 1.0

- No breaking changes expected in the 0.x to 1.0 transition
- Follow [MIGRATION_GUIDE.md](docs/MIGRATION_GUIDE.md) if available

## Reporting Issues

Found a bug? Please report it at: https://github.com/aevumdb/aevum/issues

## Contributing

Want to contribute? See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Release Schedule

- **Major releases**: Significant features and breaking changes
- **Minor releases**: New features (backward compatible)
- **Patch releases**: Bug fixes and security updates

---

For detailed information about a specific version, see the GitHub [Releases](https://github.com/aevumdb/aevum/releases) page.
