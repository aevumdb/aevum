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
 * @file rust_adapter.cpp
 * @brief Implementation of the C++ wrapper functions for the AevumDB Rust Bridge.
 *
 * @details
 * This file implements the safe C++ wrapper functions defined in `rust_adapter.hpp`.
 * It serves as the translation layer between high-level C++ objects (std::string)
 * and the low-level C-style pointers (char*) required by the Rust FFI (Foreign Function Interface).
 *
 * ## Memory Safety Strategy
 * This implementation relies strictly on the `ScopedRustString` RAII helper
 * (defined in the header) to guarantee that every heap allocation made by
 * the Rust runtime is paired with a corresponding deallocation, preventing memory leaks.
 */

#include "aevum/ffi/rust_adapter.hpp"

namespace aevum::ffi {

/**
 * @details
 * **Validation Marshalling Logic:**
 * 1. marshals C++ `std::string` arguments into raw C-strings (`const char*`).
 * 2. Invokes the exported Rust symbol `rust_validate`.
 * 3. Returns the boolean result directly to the caller.
 *
 * @note This operation is zero-copy regarding the return value.
 */
bool call_validate(const std::string& doc, const std::string& schema)
{
    return rust_validate(doc.c_str(), schema.c_str());
}

/**
 * @details
 * **Aggregation Logic:**
 * 1. Passes raw pointers of the dataset and query strings to the Rust engine.
 * 2. Returns the integer count computed by the Rust logic layer.
 *
 * This operation is read-only and does not trigger heap allocation for the result.
 */
int call_count(const std::string& data, const std::string& query)
{
    return rust_count(data.c_str(), query.c_str());
}

/**
 * @details
 * **Query Lifecycle & Memory Management:**
 * 1. **Invoke:** Calls `rust_find` with all query parameters marshaled to C-types.
 * 2. **Guard:** Immediately wraps the returned raw `char*` in a `ScopedRustString`.
 * 3. **Copy:** Constructs a deep copy `std::string` from the raw result.
 * 4. **Cleanup:** The `ScopedRustString` destructor executes automatically, invoking
 * `rust_free_string`.
 *
 * @note The `ScopedRustString::get()` method safely handles `nullptr` returns
 * by substituting them with an empty JSON object `{}`.
 */
std::string call_find(const std::string& data, const std::string& query, const std::string& sort,
                      const std::string& projection, int limit, int skip)
{
    // 1. Call Rust (Ownership of the returned pointer transfers to the C++ Host)
    char* raw_result =
        rust_find(data.c_str(), query.c_str(), sort.c_str(), projection.c_str(), limit, skip);

    // 2. Wrap in RAII guard to ensure memory is eventually freed
    ScopedRustString guard(raw_result);

    // 3. Convert to C++ string (Deep Copy) and return
    return std::string(guard.get());
}

/**
 * @details
 * **Update Execution Flow:**
 * Executes the atomic update operation in Rust and safely manages the lifecycle
 * of the returned JSON string containing the modified dataset.
 */
std::string call_update(const std::string& data, const std::string& query,
                        const std::string& update)
{
    char* raw_result = rust_update(data.c_str(), query.c_str(), update.c_str());

    // Auto-cleanup via RAII
    ScopedRustString guard(raw_result);

    return std::string(guard.get());
}

/**
 * @details
 * **Deletion Execution Flow:**
 * Executes the filtering operation in Rust and safely manages the lifecycle
 * of the returned JSON string containing the remaining dataset.
 */
std::string call_delete(const std::string& data, const std::string& query)
{
    char* raw_result = rust_delete(data.c_str(), query.c_str());

    // Auto-cleanup via RAII
    ScopedRustString guard(raw_result);

    return std::string(guard.get());
}

} // namespace aevum::ffi
