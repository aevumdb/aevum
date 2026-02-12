# Contributing to AevumDB

Thank you for considering contributing to AevumDB. Your contributions help maintain a high-performance, safe, and reliable hybrid database engine.

## Code of Conduct
By participating in this project, you agree to abide by our Code of Conduct.

## Architecture Overview
AevumDB is structured into a modular C++ kernel and a specialized Rust logic engine:
* Infra: Foundational utilities including IdGenerator and string primitives.
* FFI: The bridge layer managing cross-language ABI and memory safety.
* Storage: Persistence layer handling binary logs and WAL replay logic.
* Network: TCP server and Command Dispatcher for JSON protocols.
* aevum_logic: The Rust-based query execution and schema validation engine.

## How Can I Contribute

### Reporting Bugs
* Use the GitHub issue tracker to report bugs.
* Describe the bug clearly and include deterministic steps to reproduce.
* Include details about your environment such as OS, GCC/Clang version, and Rust toolchain version.

### Suggesting Enhancements
* Open an issue with the enhancement tag.
* Explain the use case and why this feature would be beneficial for the AevumDB ecosystem.

### Pull Requests
1. Fork the repository and create your branch from the main branch.
2. Ensure your code follows the established namespace convention such as aevum::storage or aevum::ffi.
3. If you add a new C++ feature, provide a corresponding test in the tests directory.
4. If you modify the Rust core, ensure all tests in aevum_logic/tests pass.
5. Verify that the GitHub Actions CI passes for your branch before requesting a review.

## Development Standards

### Coding Style
* C++: We use .clang-format. The build system runs this automatically, but you can run it manually to ensure compliance.
* Rust: Use cargo fmt to ensure idiomatic formatting.
* Naming: Use CamelCase for Classes and snake_case for functions and variables.

### Memory Safety and FFI
AevumDB prioritizes memory safety across the language boundary:
* In C++, strictly use RAII patterns. Manual new and delete are prohibited.
* Use the ScopedRustString helper to manage memory allocated by the Rust runtime.
* In the Rust layer, ensure all exported functions are marked #[no_mangle] and use stable ABIs.

### Testing
We use a unified testing approach via CMake and CTest. To run the complete suite including Rust logic tests:
```bash
./build.sh
cd build
ctest --output-on-failure
```
## License
By contributing to this project, you agree that your contributions will be licensed under the AevumDB Community License.
