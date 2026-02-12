/*
 * AEVUMDB COMMUNITY LICENSE
 * Version 1.0, February 2026
 *
 * Copyright (c) 2026 Ananda Firmansyah.
 * Official Organization: AevumDB (https://github.com/aevumdb)
 *
 * This source code is licensed under the AevumDB Community License.
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at the root of this repository.
 *
 * UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING, SOFTWARE
 * DISTRIBUTED UNDER THE LICENSE IS PROVIDED "AS IS", WITHOUT WARRANTY
 * OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 */

//! # AevumDB Logic Engine (FFI Layer)
//!
//! This crate implements the high-performance data processing layer for AevumDB.
//! It serves as a static library exposing a **Foreign Function Interface (FFI)**,
//! allowing the host C++ application to delegate complex JSON manipulation
//! tasks to Rust with zero-overhead integration.
//!
//! ## Core Capabilities
//!
//! * **Querying:** Advanced filtering, sorting, and pagination of JSON documents.
//! * **Mutation:** Atomic updates and deletions based on MongoDB-style query selectors.
//! * **Validation:** High-speed JSON schema validation to ensure data integrity.
//! * **Aggregation:** Statistical analysis and data reduction.
//!
//! ## FFI Architecture & Memory Management
//!
//! Communication between the C++ host and this Rust library involves crossing
//! the ABI (Application Binary Interface) boundary. Since Rust and C++ may use
//! different memory allocators (e.g., `jemalloc` vs `libc malloc`), strict
//! ownership rules must be followed to prevent memory leaks or heap corruption.
//!
//! ### The "Rust-Allocated, Rust-Freed" Protocol
//!
//! Any heap memory allocated by Rust (specifically strings returned to C++)
//! **must** be deallocated by Rust. The workflow is strictly defined as follows:
//!
//! 1. **Allocation:** Rust performs an operation, allocates a `CString`, and returns
//!    a raw pointer (`*mut c_char`) to the C++ host.
//!    * *Status: Ownership transfers to C++.*
//! 2. **Consumption:** The C++ host reads or copies the data from the pointer.
//! 3. **Deallocation:** The C++ host **must** call [`rust_free_string`] with the original pointer.
//!    * *Status: Ownership transfers back to Rust, which then safely drops the value.*

use libc::{c_char, c_int};
use std::ffi::{CStr, CString};

// Internal modules handling the business logic.
mod engine;
mod operators;

// ==================================================================================
//  HELPER FUNCTIONS (INTERNAL UTILITIES)
// ==================================================================================

/// Marshals a Rust `String` into a raw, C-compatible, null-terminated char pointer.
///
/// This function acts as a bridge for returning dynamically allocated data from
/// Rust to the C++ host. It effectively "forgets" the string in Rust so it isn't
/// dropped at the end of the current scope.
///
/// # Memory Safety
/// * **Ownership Transfer:** The caller (C++ Host) takes full ownership of the raw pointer.
/// * **Cleanup Requirement:** The caller **must** explicitly return this pointer to
///   [`rust_free_string`] to deallocate the memory. Failing to do so will result
///   in a permanent memory leak.
///
/// # Arguments
/// * `s` - The Rust `String` to be marshaled to the C interface.
///
/// # Returns
/// A raw pointer (`*mut c_char`) to the start of the null-terminated string.
fn to_c_string(s: String) -> *mut c_char {
    // Unwrap is safe here because the engine logic guarantees no internal null bytes.
    // into_raw() prevents the CString from being dropped immediately, handing control to C.
    CString::new(s).unwrap().into_raw()
}

/// Converts a raw C char pointer into a native Rust `String`.
///
/// This acts as a safe bridge for receiving string data from the C++ host.
/// If the input pointer is `NULL` or contains invalid UTF-8 sequences, this
/// function fails gracefully by returning a default empty JSON object `"{}"`
/// to prevent the database engine from crashing.
///
/// # Arguments
/// * `ptr` - A raw pointer to a null-terminated C string provided by the host.
///
/// # Returns
/// A new Rust `String` containing the data, or `"{}"` if the input was invalid/null.
fn from_c_str(ptr: *const c_char) -> String {
    if ptr.is_null() {
        // Defensive coding: Protect against null pointer dereferences from C++.
        return "{}".to_string();
    }

    // SAFETY: The unsafe block is required to dereference the raw C pointer.
    // We assume the C string is null-terminated and valid for the duration of the call.
    unsafe {
        CStr::from_ptr(ptr)
            .to_str()
            .unwrap_or("{}") // Fallback to empty JSON on UTF-8 decoding error.
            .to_string()
    }
}

// ==================================================================================
//  FFI EXPORTS (PUBLIC API)
// ==================================================================================

/// Validates a JSON document against a specific JSON schema.
///
/// This function acts as a pure validator and does not modify the underlying data.
/// It is optimized for high-throughput validation scenarios.
///
/// # Arguments
/// * `doc` - A null-terminated C string containing the JSON document to inspect.
/// * `schema` - A null-terminated C string containing the JSON schema definition.
///
/// # Returns
/// * `true` - If the document strictly adheres to the schema.
/// * `false` - If the document is invalid, or if the JSON parsing failed.
#[no_mangle]
pub extern "C" fn rust_validate(doc: *const c_char, schema: *const c_char) -> bool {
    engine::validate(&from_c_str(doc), &from_c_str(schema))
}

/// Counts the number of documents in a dataset that match a specific query.
///
/// # Arguments
/// * `data` - A null-terminated C string containing the source JSON array (the collection).
/// * `query` - A null-terminated C string containing the query object (e.g., `{"age": {"$gt": 18}}`).
///
/// # Returns
/// The count of matching documents as a signed C integer (`c_int`).
#[no_mangle]
pub extern "C" fn rust_count(data: *const c_char, query: *const c_char) -> c_int {
    engine::count(&from_c_str(data), &from_c_str(query)) as c_int
}

/// Retrieves documents from a dataset with filtering, sorting, and pagination.
///
/// This is the primary read operation for the AevumDB engine. It parses the
/// input JSON, applies the query logic, and serializes the result back to a string.
///
/// # Arguments
/// * `data` - Source JSON array (C string).
/// * `query` - Filter criteria (C string).
/// * `sort` - Sort criteria (C string, e.g., `{"name": 1}`).
/// * `projection` - Fields to include or exclude (C string, e.g., `{"password": 0}`).
/// * `limit` - The maximum number of documents to return. Values `< 0` are treated as 0.
/// * `skip` - The number of documents to bypass before returning results. Values `< 0` are treated as 0.
///
/// # Returns
/// A raw pointer to a C string containing the result JSON array.
///
/// # Safety
/// **CRITICAL:** The returned pointer represents a heap allocation managed by Rust.
/// The caller **must** pass this pointer to [`rust_free_string`] when finished using it.
#[no_mangle]
pub extern "C" fn rust_find(
    data: *const c_char,
    query: *const c_char,
    sort: *const c_char,
    projection: *const c_char,
    limit: c_int,
    skip: c_int,
) -> *mut c_char {
    // Sanitize integer inputs to prevent potential buffer underflows or logic errors.
    let l = if limit < 0 { 0 } else { limit as usize };
    let s = if skip < 0 { 0 } else { skip as usize };

    to_c_string(engine::find(
        &from_c_str(data),
        &from_c_str(query),
        &from_c_str(sort),
        &from_c_str(projection),
        l,
        s,
    ))
}

/// Modifies documents in the dataset that match the selection criteria.
///
/// Supports atomic operators like `$set`, `$unset`, `$inc`, `$push`, and `$pull`.
///
/// # Arguments
/// * `data` - Source JSON array (C string).
/// * `query` - Selection criteria to identify documents to update.
/// * `update` - The modification rules to apply (e.g., `{"$set": {"active": true}}`).
///
/// # Returns
/// A raw pointer to a C string containing the *entire* dataset with updates applied.
///
/// # Safety
/// The returned pointer is a new allocation. The caller **must** free it using [`rust_free_string`].
#[no_mangle]
pub extern "C" fn rust_update(
    data: *const c_char,
    query: *const c_char,
    update: *const c_char,
) -> *mut c_char {
    to_c_string(engine::update(
        &from_c_str(data),
        &from_c_str(query),
        &from_c_str(update),
    ))
}

/// Removes documents from the dataset that match the selection criteria.
///
/// # Arguments
/// * `data` - Source JSON array (C string).
/// * `query` - Selection criteria to identify documents to delete.
///
/// # Returns
/// A raw pointer to a C string containing the remaining documents in the dataset.
///
/// # Safety
/// The returned pointer is a new allocation. The caller **must** free it using [`rust_free_string`].
#[no_mangle]
pub extern "C" fn rust_delete(data: *const c_char, query: *const c_char) -> *mut c_char {
    to_c_string(engine::delete(&from_c_str(data), &from_c_str(query)))
}

// ==================================================================================
//  MEMORY MANAGEMENT EXPORTS
// ==================================================================================

/// Deallocates a C string previously allocated by this Rust library.
///
/// This function reclaims ownership of the raw pointer and drops the associated
/// `CString`, freeing the memory back to the system allocator.
///
/// # Usage
/// Call this function exactly once for every pointer returned by [`rust_find`],
/// [`rust_update`], or [`rust_delete`].
///
/// # Arguments
/// * `s` - The raw pointer to the string to be freed.
///
/// # Safety
/// * **Validity:** `s` must be a pointer previously returned by a function in this crate.
/// * **Double Free:** `s` must not have been freed already. Double-freeing leads to undefined behavior.
/// * **Null Pointers:** This function safely handles `NULL` pointers (no-op).
#[no_mangle]
pub unsafe extern "C" fn rust_free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe {
            // RECLAIM OWNERSHIP:
            // CString::from_raw takes the pointer and reconstructs the CString object.
            // When the variable `_` goes out of scope at the end of this block,
            // Rust's drop semantics will automatically deallocate the memory.
            let _ = CString::from_raw(s);
        }
    }
}
