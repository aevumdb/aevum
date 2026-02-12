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
# Mewarisi header dari direktori include utama, internal tests, dan vendor cJSON
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)

# --- Test Source Management ---
# Mengumpulkan semua unit test (infra, ffi, storage, network) secara rekursif
file(GLOB_RECURSE TEST_SOURCES "*.cpp")

# Validasi awal: Pastikan suite pengujian tidak kosong
if(NOT TEST_SOURCES)
    message(FATAL_ERROR "  [CRIT] No test source files (.cpp) detected in tests directory.")
endif()

# --- Test Build Targets ---
# Membuat executable 'aevum_tests' sebagai runner utama.
# PROJECT_SOURCES dan CJSON_SOURCES diwariskan dari root CMakeLists.txt.
add_executable(aevum_tests 
    ${TEST_SOURCES} 
    ${PROJECT_SOURCES} 
    ${CJSON_SOURCES}
)

# --- Link Libraries ---
# Menghubungkan test runner dengan modul statis Rust dan dependensi sistem operasi.
# 
target_link_libraries(aevum_tests 
    PRIVATE 
    ${RUST_STATIC_LIB} 
    ${CMAKE_DL_LIBS}
    Threads::Threads
    pthread
)

# --- CTest Integration ---
# Mendaftarkan executable ke dalam CTest framework.
# Memungkinkan otomasi pengujian via perintah 'ctest' pada pipeline CI/CD.
add_test(NAME RunKernelTests COMMAND aevum_tests)

# --- Post-Configuration Diagnostics ---
message(STATUS "  Target 'aevum_tests' built for: x86_64-linux-gnu")
message(STATUS "  Integrated Modules: infra, ffi, storage, network")
message(STATUS "  Environment: Pure RAII, Binary AOL, Rust-Logic")
message(STATUS "───────────────────────────────────────────────────")
