# ------------------------------------------------------------------------------
# AEVUMDB | Unit Testing Configuration
# Framework for Kernel C++ and Rust Logic Integration Tests
# ------------------------------------------------------------------------------
project(aevum_tests)

# --- Visual Branding ---
message(STATUS "───────────────────────────────────────────────────")
message(STATUS "  Configuring AevumDB Kernel Test Suite")
message(STATUS "───────────────────────────────────────────────────")

# --- Include & Dependencies ---
# Inherit headers from main include directory, internal tests, and cJSON vendor
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)

# --- Test Source Management ---
# Collect all unit tests (infra, ffi, storage, network) recursively
file(GLOB_RECURSE TEST_SOURCES "*.cpp")

# Initial validation: Ensure the test suite is not empty
if(NOT TEST_SOURCES)
    message(FATAL_ERROR "  [CRIT] No test source files (.cpp) detected in tests directory.")
endif()

# --- Test Build Targets ---
# Create 'aevum_tests' executable as the main runner.
# PROJECT_SOURCES and CJSON_SOURCES are inherited from the root CMakeLists.txt.
add_executable(aevum_tests
    ${TEST_SOURCES}
    ${PROJECT_SOURCES}
    ${CJSON_SOURCES}
)

# Ensure the Rust static library is built before compiling and linking the C++ test executable.
add_dependencies(aevum_tests rust_build)
# --- Link Libraries ---
# Link the test runner with the Rust static module and operating system dependencies. 
target_link_libraries(aevum_tests 
    PRIVATE 
    ${RUST_STATIC_LIB} 
    ${CMAKE_DL_LIBS}
    Threads::Threads
    pthread
)

# --- CTest Integration ---
# Register the executable within the CTest framework.
# Enables test automation via 'ctest' command in CI/CD pipelines.
add_test(NAME RunKernelTests COMMAND aevum_tests)

# --- Post-Configuration Diagnostics ---
message(STATUS "  Target 'aevum_tests' built for: x86_64-linux-gnu")
message(STATUS "  Integrated Modules: infra, ffi, storage, network")
message(STATUS "  Environment: Pure RAII, Binary AOL, Rust-Logic")
message(STATUS "───────────────────────────────────────────────────")
