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
 * @file rust_adapter.hpp
 * @brief Foreign Function Interface (FFI) Bridge between C++ and Rust.
 *
 * @details
 * This header serves as the critical boundary layer between the AevumDB C++ host
 * application and the `aevum_logic` Rust core. It abstracts the complexities of
 * cross-language ABI (Application Binary Interface) calls, type marshaling, and
 * memory ownership protocols.
 *
 * ## Architecture
 * 1. **Raw Interface (`extern "C"`)**: Declares the raw symbols exported by the
 * Rust static library. These functions operate on C-style pointers (`char*`)
 * and require strict manual memory management.
 * 2. **Safe Wrapper (`aevum::ffi`)**: Provides a high-level, idiomatic C++17 interface.
 * It implements the "Rust-Allocated, Rust-Freed" protocol automatically using
 * RAII (Resource Acquisition Is Initialization) patterns.
 *
 * @note
 * All functions in `aevum::ffi` are exception-safe and guarantee that memory
 * allocated by the Rust runtime is correctly freed, preventing memory leaks.
 */

#pragma once

#include <cstring>
#include <stdexcept>
#include <string>

// ============================================================================
//  RAW FFI DECLARATIONS (Low-Level)
// ============================================================================
//  These functions map 1:1 to the `#[no_mangle]` entry points in the Rust core.
//  WARNING: Direct invocation is discouraged. Use the `aevum::ffi` namespace.
// ============================================================================

extern "C" {

/**
 * @brief Low-level interface to validate a JSON document against a schema.
 *
 * @param doc A null-terminated C-string containing the JSON document.
 * @param schema A null-terminated C-string containing the JSON schema.
 * @return true if valid, false otherwise.
 */
bool rust_validate(const char* doc, const char* schema);

/**
 * @brief Low-level interface to count matching documents.
 *
 * @param data A null-terminated C-string containing the dataset.
 * @param query A null-terminated C-string containing the filter criteria.
 * @return The integer count of matches.
 */
int rust_count(const char* data, const char* query);

/**
 * @brief Low-level interface to execute a FIND query.
 *
 * @param data Source JSON array.
 * @param query Filter criteria.
 * @param sort Sort criteria.
 * @param projection Field selection.
 * @param limit Max records (integer).
 * @param skip Offset (integer).
 *
 * @return A raw pointer to a Rust-allocated heap string.
 * @warning **MEMORY SAFETY**: The caller OWNS this pointer. It MUST be returned
 * to Rust via `rust_free_string` to avoid memory leaks.
 */
char* rust_find(const char* data, const char* query, const char* sort, const char* projection,
                int limit, int skip);

/**
 * @brief Low-level interface to execute an UPDATE query.
 *
 * @param data Source JSON array.
 * @param query Filter criteria.
 * @param update Update operations.
 *
 * @return A raw pointer to a Rust-allocated heap string.
 * @warning **MEMORY SAFETY**: The caller OWNS this pointer and MUST free it using
 * `rust_free_string`.
 */
char* rust_update(const char* data, const char* query, const char* update);

/**
 * @brief Low-level interface to execute a DELETE query.
 *
 * @param data Source JSON array.
 * @param query Filter criteria.
 *
 * @return A raw pointer to a Rust-allocated heap string.
 * @warning **MEMORY SAFETY**: The caller OWNS this pointer and MUST free it using
 * `rust_free_string`.
 */
char* rust_delete(const char* data, const char* query);

/**
 * @brief Releases memory allocated by the Rust allocator.
 *
 * @param s The pointer originally returned by a `rust_*` function.
 * It is safe to pass `nullptr`.
 */
void rust_free_string(char* s);

} // extern "C"

// ============================================================================
//  SAFE C++ WRAPPER (High-Level)
// ============================================================================

namespace aevum::ffi {

/**
 * @class ScopedRustString
 * @brief Internal RAII helper to ensure Rust memory is always freed.
 *
 * This class acts as a smart pointer guard for strings allocated by the Rust logic layer.
 * When this object goes out of scope (e.g., function return or exception),
 * its destructor automatically invokes `rust_free_string`.
 */
class ScopedRustString {
  public:
    /// Takes ownership of a raw pointer returned by Rust.
    explicit ScopedRustString(char* raw) : ptr_(raw) {}

    // Delete copy constructor and assignment operator to prevent double-freeing.
    ScopedRustString(const ScopedRustString&) = delete;
    ScopedRustString& operator=(const ScopedRustString&) = delete;

    /// Destructor automatically releases memory back to Rust.
    ~ScopedRustString()
    {
        if (ptr_) {
            rust_free_string(ptr_);
        }
    }

    /// Accessor for the underlying C-string. Returns "{}" if null.
    const char* get() const
    {
        return ptr_ ? ptr_ : "{}";
    }

    /// Converts the underlying C-string to a standard C++ string.
    std::string to_string() const
    {
        return std::string(get());
    }

  private:
    char* ptr_;
};

// ------------------------------------------------------------------------
// Public FFI Function Declarations
// ------------------------------------------------------------------------

/**
 * @brief Validates a document against a schema.
 *
 * @param doc The JSON document string.
 * @param schema The JSON schema definition.
 * @return true if the document strictly adheres to the schema.
 */
bool call_validate(const std::string& doc, const std::string& schema);

/**
 * @brief Counts documents matching a specific query.
 *
 * @param data The complete JSON dataset.
 * @param query The filter criteria.
 * @return The number of matching documents.
 */
int call_count(const std::string& data, const std::string& query);

/**
 * @brief Retrieves data with filtering, sorting, and pagination.
 *
 * This function bridges the C++ `std::string` world with the Rust raw pointer world.
 * It handles the allocation lifecycle automatically.
 *
 * @param data The complete JSON dataset.
 * @param query Filter criteria (e.g., `{"age": {"$gt": 18}}`).
 * @param sort Sort order (e.g., `{"name": 1}`).
 * @param projection Fields to include (e.g., `{"password": 0}`).
 * @param limit Max records to return.
 * @param skip Offset records.
 * @return A native C++ `std::string` containing the result JSON.
 */
std::string call_find(const std::string& data, const std::string& query, const std::string& sort,
                      const std::string& projection, int limit, int skip);

/**
 * @brief Modifications: Updates documents based on criteria.
 *
 * @param data The complete JSON dataset.
 * @param query Filter criteria.
 * @param update Update operations (e.g., `{"$set": ...}`).
 * @return The full updated dataset as a JSON string.
 */
std::string call_update(const std::string& data, const std::string& query,
                        const std::string& update);

/**
 * @brief Modifications: Deletes documents based on criteria.
 *
 * @param data The complete JSON dataset.
 * @param query Filter criteria.
 * @return The remaining dataset as a JSON string.
 */
std::string call_delete(const std::string& data, const std::string& query);

} // namespace aevum::ffi
