# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachanglog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.1] - 2026-02-12

### Fixed
- Corrected compilation error in `aevum_logic/src/lib.rs` by marking the `rust_free_string` FFI function as `unsafe`.
- Wrapped calls to `aevum_logic::rust_free_string` in `unsafe` blocks within Rust test files to comply with `rust_free_string`'s new `unsafe` declaration.

## [1.0.0] - 2026-02-12

### Added
- Initial release of AevumDB, a high-performance, embedded NoSQL database engine.
- Hybrid kernel architecture leveraging C++17 for I/O/networking and Rust for memory safety and complex query logic.
- Core database features: JSON document storage, CRUD operations, in-memory performance, append-only persistence, indexing, schema validation, and thread-safety.
- Standalone server for network interaction.
- C++ API for embedding the database.
- Comprehensive `README.md` for project overview, setup, and usage.
- Initial documentation in `docs/` directory covering project overview and testing instructions.
- GitHub Actions CI workflow for automated build, linting, and testing of C++ and Rust components.
- Standard project metadata: `LICENSE`, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md`, and `CHANGELOG.md`.