# Testing AevumDB

AevumDB utilizes a hybrid C++ and Rust architecture, and as such, has separate testing procedures for each component.

## Running Tests

### Rust Component Tests

The Rust-based logic of AevumDB includes unit and integration tests that can be run using Cargo.

To execute all Rust tests:

```sh
cd aevum_logic
cargo test
```

This command will compile and run all tests defined within the `aevum_logic` crate.

### C++ Component Tests

The C++ components of AevumDB use CMake and CTest for their testing framework.

1.  **Ensure Project is Built**:
    Before running C++ tests, make sure the project has been built. If you used the `build.sh` script, your project is likely already built. Otherwise, follow the manual CMake build steps in the [README.md](../README.md).

    ```sh
    ./build.sh # Or your manual build steps
    ```

2.  **Navigate to Build Directory**:
    C++ tests are typically run from the build directory.

    ```sh
    cd build
    ```

3.  **Execute Tests with CTest**:
    CTest is the CMake test driver program. It can discover and run all tests defined in the CMake project.

    ```sh
    ctest
    ```

    To see more detailed output during testing (e.g., stdout from test executables), you can use:

    ```sh
    ctest --output-on-failure
    ```

4.  **Running Individual C++ Test Executables**:
    You can also run specific C++ test executables directly from the `build/tests` directory. For example:

    ```sh
    ./tests/ffi_test
    ./tests/handler_test
    # ... and so on for other test executables
    ```

    Note that the exact names of the test executables can be found by listing the contents of the `build/tests/` directory after a successful build.

## Continuous Integration

Refer to the `.github/workflows/ci.yml` file for how tests are executed in the continuous integration pipeline.
