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
 * @file framework.hpp
 * @brief A lightweight, header-only unit testing micro-framework for AevumDB.
 *
 * @details
 * This utility provides a minimal footprint infrastructure for validating C++
 * kernel components. It features ANSI-colored terminal output, exception-protected
 * execution blocks, and standardized assertion macros for internal consistency.
 */

#pragma once

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace aevum::test {

// ========================================================================
// Global Metrics
// ========================================================================

inline int passed_count = 0; ///< Cumulative successful test counter.
inline int failed_count = 0; ///< Cumulative failed test counter.

// ========================================================================
// Assertion Primitives
// ========================================================================

/**
 * @brief Validates that two generic values are equivalent.
 */
template <typename T> void assert_eq(T val1, T val2, const char* file, int line, const char* expr)
{
    if (val1 != val2) {
        std::cout << "\033[31m[FAIL]\033[0m " << file << ":" << line
                  << " -> Assertion failed: " << expr << " (" << val1 << " != " << val2 << ")"
                  << std::endl;
        failed_count++;
        throw std::runtime_error("Assertion failed");
    }
}

/**
 * @brief Validates that two generic values are NOT equivalent.
 */
template <typename T> void assert_ne(T val1, T val2, const char* file, int line, const char* expr)
{
    if (val1 == val2) {
        std::cout << "\033[31m[FAIL]\033[0m " << file << ":" << line
                  << " -> Assertion failed: " << expr << " (" << val1 << " == " << val2 << ")"
                  << std::endl;
        failed_count++;
        throw std::runtime_error("Assertion failed");
    }
}

/**
 * @brief Validates that a boolean expression evaluates to true.
 */
inline void assert_true(bool cond, const char* file, int line, const char* expr)
{
    if (!cond) {
        std::cout << "\033[31m[FAIL]\033[0m " << file << ":" << line
                  << " -> Assertion failed: " << expr << " is FALSE" << std::endl;
        failed_count++;
        throw std::runtime_error("Assertion failed");
    }
}

/**
 * @brief Validates that a boolean expression evaluates to false.
 */
inline void assert_true_false(bool cond, const char* file, int line, const char* expr)
{
    if (cond) {
        std::cout << "\033[31m[FAIL]\033[0m " << file << ":" << line
                  << " -> Assertion failed: " << expr << " is TRUE" << std::endl;
        failed_count++;
        throw std::runtime_error("Assertion failed");
    }
}

// ========================================================================
// Execution Orchestrator
// ========================================================================

/**
 * @brief Executes a test case within a protected execution context.
 */
inline void run(std::string_view name, std::function<void()> func)
{
    std::cout << "[RUN  ] " << name << "... " << std::flush;
    try {
        func();
        // Clear line and print pass status
        std::cout << "\r\033[32m[PASS]\033[0m " << name << "          " << std::endl;
        passed_count++;
    } catch (...) {
        // Status failed is already printed by assertion primitives
        std::cout << "\r\033[31m[FAIL]\033[0m " << name << "          " << std::endl;
    }
}

/**
 * @brief Emits a summary report of the current test session.
 */
inline void print_summary()
{
    std::cout << "\n\033[36m=== AevumDB Kernel Test Summary ===\033[0m" << std::endl;
    std::cout << "Passed: " << passed_count << std::endl;
    if (failed_count > 0) {
        std::cout << "Failed: \033[31m" << failed_count << "\033[0m" << std::endl;
    } else {
        std::cout << "Failed: 0" << std::endl;
    }
    std::cout << "Total:  " << (passed_count + failed_count) << std::endl;
}

} // namespace aevum::test

// ============================================================================
// API Macros
// ============================================================================

/**
 * @def ASSERT_EQ
 * @brief Macro for equality assertions. Includes file and line metadata.
 */
#define ASSERT_EQ(a, b) aevum::test::assert_eq((a), (b), __FILE__, __LINE__, #a " == " #b)

/**
 * @def ASSERT_NE
 * @brief Macro for inequality assertions. Includes file and line metadata.
 */
#define ASSERT_NE(a, b) aevum::test::assert_ne((a), (b), __FILE__, __LINE__, #a " != " #b)

/**
 * @def ASSERT_TRUE
 * @brief Macro for truthiness assertions.
 */
#define ASSERT_TRUE(a) aevum::test::assert_true((a), __FILE__, __LINE__, #a)

/**
 * @def ASSERT_FALSE
 * @brief Macro for falsiness assertions.
 */
#define ASSERT_FALSE(a) aevum::test::assert_true_false((a), __FILE__, __LINE__, #a)

/**
 * @def RUN_TEST
 * @brief Orchestrates the execution of a named test function.
 */
#define RUN_TEST(func_name) aevum::test::run(#func_name, func_name)
