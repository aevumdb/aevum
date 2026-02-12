/*
 * AEVUMDB COMMUNITY LICENSE
 * Version 1.0, February 2026
 *
 * Copyright (c) 2026 Ananda Firmansyah.
 * Official Organization: AevumDB (https://github.com/aevumdb)
 *
 * This source code is licensed under the AevumDB Community License.
 * You may not use this file except in compliance with the License.
 */

/**
 * @file main_test.cpp
 * @brief Central orchestrator for the AevumDB Kernel Test Suite.
 *
 * @details
 * This file serves as the main entry point for the testing environment. It
 * aggregates unit and integration tests across all core subsytems:
 * Infrastructure, FFI Bridge, Storage Engine, and Network Protocol.
 */

#include "framework.hpp"

#include <iostream>

// ============================================================================
// Forward Declarations
// ============================================================================
// The following test functions are implemented in their respective
// translation units (e.g., infra_test.cpp, ffi_test.cpp, etc.).

// Infrastructure Subsystem (infra_test.cpp)
void test_uuid_length();
void test_uuid_uniqueness();
void test_string_trim();
void test_string_trim_empty();

// Rust FFI Bridge Subsystem (ffi_test.cpp)
void test_rust_find_logic();
void test_rust_update_logic();
void test_rust_validate_logic();

// Storage Engine Subsystem (storage_test.cpp)
void test_db_insert();
void test_db_persistence();

// Network Protocol Subsystem (handler_test.cpp)
void test_handle_insert_request();
void test_handle_invalid_json();

/**
 * @brief Test Suite Execution Entry Point.
 *
 * Orchestrates the sequential execution of registered test cases.
 *
 * @return
 * - 0: All tests passed (Success).
 * - 1: One or more assertions failed (Exit failure for CI pipelines).
 */
int main()
{
    std::cout << "\033[36mInitiating AevumDB Kernel Test Suite...\033[0m" << std::endl;

    // --- 1. Infrastructure Subsystem Tests ---
    // Verifies the foundational blocks (ID Generator and String Primitives).
    RUN_TEST(test_uuid_length);
    RUN_TEST(test_uuid_uniqueness);
    RUN_TEST(test_string_trim);
    RUN_TEST(test_string_trim_empty);

    // --- 2. Rust FFI Bridge Subsystem Tests ---
    // Verifies data marshaling and the Rust core execution logic.

    RUN_TEST(test_rust_find_logic);
    RUN_TEST(test_rust_update_logic);
    RUN_TEST(test_rust_validate_logic);

    // --- 3. Storage Engine Subsystem Tests ---
    // Verifies physical persistence, WAL replay, and CRUD atomicity.

    RUN_TEST(test_db_insert);
    RUN_TEST(test_db_persistence);

    // --- 4. Network Protocol Subsystem Tests ---
    // Verifies the Command Dispatcher (JSON-In -> DB-Execute -> JSON-Out).
    RUN_TEST(test_handle_insert_request);
    RUN_TEST(test_handle_invalid_json);

    // Render the final results summary to stdout.
    aevum::test::print_summary();

    // Signal exit status: Non-zero if failures occurred.
    return (aevum::test::failed_count == 0) ? 0 : 1;
}
