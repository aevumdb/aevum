# ------------------------------------------------------------------------------
# AEVUMDB | Main CMake Build System
# ------------------------------------------------------------------------------
# Target: High-performance C++/Rust Hybrid NoSQL Engine
# Author: Ananda Firmansyah
# Version: 1.1.1 (Aevum Community License)
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.15)

project(aevum 
    VERSION 1.1.1
    LANGUAGES C CXX
)

# --- 1. System Dependencies ---
# Ensure thread support for the multi-threaded Scheduler and Network Server.
find_package(Threads REQUIRED)

message(STATUS "───────────────────────────────────────────────────")
message(STATUS "  Initializing AevumDB Build System v${PROJECT_VERSION}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS "───────────────────────────────────────────────────")

# --- 2. C++ Compiler Configuration ---
# C++17 is required for std::filesystem, shared_mutex, and modern RAII.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable colored diagnostics for GCC and Clang.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fdiagnostics-color=always -Wall -Wextra)
endif()

# --- 3. Style Enforcement (Clang-Format) ---
find_program(CLANG_FORMAT_EXE clang-format)
if(CLANG_FORMAT_EXE)
    add_custom_target(auto_format
        COMMAND find ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests 
                     -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) 
                     | xargs ${CLANG_FORMAT_EXE} -i -style=file
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Enforcing AevumDB code style (Clang-Format)..."
        VERBATIM
    )
endif()

# --- 4. Rust Core Logic Integration (Cargo) ---
# AevumDB uses Rust for high-level query execution and schema validation.
set(RUST_PROJECT_DIR "${CMAKE_SOURCE_DIR}/aevum_logic")
set(RUST_STATIC_LIB "${RUST_PROJECT_DIR}/target/release/libaevum_logic.a")

# Monitor Rust sources to handle incremental builds efficiently.
file(GLOB_RECURSE RUST_SOURCES "${RUST_PROJECT_DIR}/src/*.rs")

add_custom_command(
    OUTPUT ${RUST_STATIC_LIB}
    COMMAND cargo build --release
    WORKING_DIRECTORY ${RUST_PROJECT_DIR}
    DEPENDS ${RUST_SOURCES} ${RUST_PROJECT_DIR}/Cargo.toml
    COMMENT "Building Aevum Logic Engine (Rust Release Artifact)..."
    VERBATIM
)

add_custom_target(rust_build DEPENDS ${RUST_STATIC_LIB})

# --- 5. Project Layout & Search Paths ---
include_directories(
    include 
    third_party/cJSON
)

# Collect kernel source files.
file(GLOB_RECURSE PROJECT_SOURCES "src/*.cpp")
file(GLOB CJSON_SOURCES "third_party/cJSON/cJSON.c")

# Isolate the main entry point for cleaner linking with the test suite.
set(APP_ENTRY "src/main.cpp")
list(REMOVE_ITEM PROJECT_SOURCES ${CMAKE_SOURCE_DIR}/${APP_ENTRY})

# --- 6. Main Server Target ---
add_executable(aevum_server ${APP_ENTRY} ${PROJECT_SOURCES} ${CJSON_SOURCES})

# Execution order: Format -> Build Rust -> Compile C++.
if(CLANG_FORMAT_EXE)
    add_dependencies(aevum_server auto_format rust_build)
else()
    add_dependencies(aevum_server rust_build)
endif()

target_link_libraries(aevum_server 
    PRIVATE 
    ${RUST_STATIC_LIB} 
    ${CMAKE_DL_LIBS}
    Threads::Threads
)

# --- 7. Testing Infrastructure ---
enable_testing()

# A. C++ Integration Tests
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

# B. Rust Internal Logic Tests (Cargo Test Integration)
# These tests run the logic engine validation files in aevum_logic/tests/
if(EXISTS "${RUST_PROJECT_DIR}")
    # 1. Pipeline Verification
    add_test(NAME Rust_Engine_Pipeline 
             COMMAND cargo test --test engine_pipeline
             WORKING_DIRECTORY ${RUST_PROJECT_DIR})

    # 2. FFI Query Lifecycle Verification
    add_test(NAME Rust_FFI_Lifecycle 
             COMMAND cargo test --test test_ffi_full_query_lifecycle
             WORKING_DIRECTORY ${RUST_PROJECT_DIR})

    # 3. Numeric Operators Verification
    add_test(NAME Rust_Numeric_Operators 
             COMMAND cargo test --test test_ffi_numeric_range_operators
             WORKING_DIRECTORY ${RUST_PROJECT_DIR})
endif()

# --- 8. Build Summary ---
message(STATUS "  Modules Loaded: Infra, FFI, Storage, Network, Rust-Core")
message(STATUS "  Ready for compilation. Use 'make' and 'ctest' to verify.")
message(STATUS "───────────────────────────────────────────────────")
