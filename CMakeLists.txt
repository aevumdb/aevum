# Project Configuration
cmake_minimum_required(VERSION 3.21)
project(aevumdb VERSION 1.2.2 LANGUAGES CXX C ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build Optimizations
set(CMAKE_UNITY_BUILD ON)
set(CMAKE_UNITY_BUILD_BATCH_SIZE 8)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# Output Directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Third-Party Dependencies (Persistent Vendor Strategy)
include(ExternalProject)
set(AEVUM_THIRD_PARTY_DIST "${CMAKE_SOURCE_DIR}/third_party/dist")
file(MAKE_DIRECTORY "${AEVUM_THIRD_PARTY_DIST}")
file(MAKE_DIRECTORY "${AEVUM_THIRD_PARTY_DIST}/include")
file(MAKE_DIRECTORY "${AEVUM_THIRD_PARTY_DIST}/include/libbson-1.0")
file(MAKE_DIRECTORY "${AEVUM_THIRD_PARTY_DIST}/lib")

# 1. simdjson
add_subdirectory(third_party/simdjson EXCLUDE_FROM_ALL)

# 2. libbson
set(BSON_STATIC_LIB "${AEVUM_THIRD_PARTY_DIST}/lib/libbson-static-1.0.a")
if(NOT EXISTS "${BSON_STATIC_LIB}")
    message(STATUS "Vendor: libbson not found. Building...")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(ENABLE_MONGOC OFF CACHE BOOL "" FORCE)
    set(ENABLE_BSON ON CACHE BOOL "" FORCE)
    set(ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ENABLE_STATIC ON CACHE BOOL "" FORCE)
    set(CMAKE_INSTALL_PREFIX "${AEVUM_THIRD_PARTY_DIST}" CACHE PATH "" FORCE)
    add_subdirectory(third_party/mongo-c-driver EXCLUDE_FROM_ALL)
else()
    add_library(bson_static STATIC IMPORTED)
    set_target_properties(bson_static PROPERTIES
        IMPORTED_LOCATION "${BSON_STATIC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${AEVUM_THIRD_PARTY_DIST}/include/libbson-1.0"
    )
endif()

# 3. WiredTiger
set(WIREDTIGER_STATIC_LIB "${AEVUM_THIRD_PARTY_DIST}/lib/libwiredtiger.a")
if(NOT EXISTS "${WIREDTIGER_STATIC_LIB}")
    message(STATUS "Vendor: WiredTiger not found. Initiating optimized build...")
    ExternalProject_Add(wiredtiger_project
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/wiredtiger"
        PREFIX "${CMAKE_BINARY_DIR}/third_party/wiredtiger"
        INSTALL_DIR "${AEVUM_THIRD_PARTY_DIST}"
        PATCH_COMMAND sed -i "/^# /! s/add_subdirectory(test\\/fail_fs)/# add_subdirectory(test\\/fail_fs)/g" ext/CMakeLists.txt && sed -i "/^# /! s/add_subdirectory(test\\/key_provider)/# add_subdirectory(test\\/key_provider)/g" ext/CMakeLists.txt
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${AEVUM_THIRD_PARTY_DIST}
            -G ${CMAKE_GENERATOR}
            -DENABLE_SHARED:BOOL=OFF
            -DENABLE_STATIC:BOOL=ON
            -DENABLE_PYTHON:BOOL=OFF
            -DHAVE_UNITTEST:BOOL=OFF
            -DENABLE_STRICT:BOOL=OFF
            -DENABLE_TESTING:BOOL=OFF
            -DENABLE_EXAMPLES:BOOL=OFF
            -DENABLE_EXTENSIONS:BOOL=OFF
            -DWITH_PIC:BOOL=ON
            -DENABLE_ZLIB:BOOL=OFF
            -DENABLE_SNAPPY:BOOL=OFF
            # Suppress specific GCC warnings that trigger errors in WiredTiger on newer compilers
            "-DCMAKE_C_FLAGS=-Wno-array-bounds -Wno-error ${CMAKE_C_FLAGS}"
            -DCMAKE_BUILD_TYPE:STRING=Release
            -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
        BUILD_BYPRODUCTS "${WIREDTIGER_STATIC_LIB}"
    )
    add_library(wt_static STATIC IMPORTED)
    set_target_properties(wt_static PROPERTIES
        IMPORTED_LOCATION "${WIREDTIGER_STATIC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${AEVUM_THIRD_PARTY_DIST}/include"
    )
    add_dependencies(wt_static wiredtiger_project)
else()
    add_library(wt_static STATIC IMPORTED)
    set_target_properties(wt_static PROPERTIES
        IMPORTED_LOCATION "${WIREDTIGER_STATIC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${AEVUM_THIRD_PARTY_DIST}/include"
    )
    add_custom_target(wiredtiger_project)
endif()

set(HAVE_WIREDTIGER TRUE)
add_definitions(-DHAVE_WIREDTIGER)

# Rust FFI Integration
find_program(CARGO_EXECUTABLE cargo)
if(CARGO_EXECUTABLE)
    set(RUST_FFI_DIR "${CMAKE_SOURCE_DIR}/src/aevum/ffi")
    set(RUST_FFI_LIB "${RUST_FFI_DIR}/target/release/libaevum_ffi.a")
    add_custom_command(
        OUTPUT "${RUST_FFI_LIB}"
        COMMAND ${CARGO_EXECUTABLE} build --release
        WORKING_DIRECTORY "${RUST_FFI_DIR}"
        COMMENT "Building Rust FFI..."
        VERBATIM
    )
    add_custom_target(rust_ffi_target DEPENDS "${RUST_FFI_LIB}")
endif()

# Target Discovery
include_directories(${CMAKE_SOURCE_DIR}/src)
file(GLOB_RECURSE ALL_SOURCES "${CMAKE_SOURCE_DIR}/src/aevum/**/*.cpp")
list(FILTER ALL_SOURCES EXCLUDE REGEX "main\\.cpp$")

set(DAEMON_MAIN "${CMAKE_SOURCE_DIR}/src/aevum/main.cpp")
set(SHELL_MAIN "${CMAKE_SOURCE_DIR}/src/aevum/shell/main.cpp")

# Core Library Components
# We separate linenoise (C) from the main core (C++) to avoid PCH conflicts.
add_library(linenoise STATIC "${CMAKE_SOURCE_DIR}/third_party/linenoise/linenoise.c")
target_include_directories(linenoise PUBLIC "${CMAKE_SOURCE_DIR}/third_party/linenoise")

# Main AevumDB Core Library (C++)
add_library(aevum_core STATIC ${ALL_SOURCES})
add_dependencies(aevum_core wiredtiger_project)

if(CARGO_EXECUTABLE)
    add_dependencies(aevum_core rust_ffi_target)
endif()

# Optimized Precompiled Headers for C++
target_precompile_headers(aevum_core PRIVATE
    <iostream>
    <string>
    <string_view>
    <vector>
    <memory>
    <map>
    <unordered_map>
    <functional>
    <algorithm>
    <mutex>
    <shared_mutex>
    <thread>
    <chrono>
    <bson/bson.h>
    <simdjson.h>
)

target_include_directories(aevum_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(aevum_core PUBLIC linenoise simdjson bson_static wt_static)

if(CARGO_EXECUTABLE)
    target_link_libraries(aevum_core PUBLIC "${RUST_FFI_LIB}")
endif()

# Executables
add_executable(aevumdb ${DAEMON_MAIN})
target_link_libraries(aevumdb PRIVATE aevum_core pthread m dl rt)

add_executable(aevumsh ${SHELL_MAIN})
target_link_libraries(aevumsh PRIVATE aevum_core pthread m dl rt)

if(NOT MSVC)
    target_compile_options(aevum_core PRIVATE -Wall -Wextra -O3)
    target_compile_options(aevumdb PRIVATE -Wall -Wextra -O3)
    target_compile_options(aevumsh PRIVATE -Wall -Wextra -O3)
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS aevumdb aevumsh
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Summary
message(STATUS "")
message(STATUS " AevumDB v${PROJECT_VERSION} Configuration Summary")
message(STATUS "")
message(STATUS " Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS " Generator:    ${CMAKE_GENERATOR}")
message(STATUS " ccache:       ${CCACHE_PROGRAM}")
message(STATUS " Unity Build:  ${CMAKE_UNITY_BUILD}")
message(STATUS " Install Path: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
