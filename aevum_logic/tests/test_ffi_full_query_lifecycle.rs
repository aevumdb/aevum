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

#[cfg(test)]
mod tests {
    use aevum_logic;
    use std::ffi::{CStr, CString};
    use std::os::raw::c_char;

    // ==================================================================================
    //  TEST INFRASTRUCTURE
    // ==================================================================================

    /// Allocates a C-compatible string on the heap and returns a raw pointer.
    ///
    /// # Memory Safety
    /// This function performs a raw heap allocation. The caller assumes full responsibility
    /// for deallocating this memory using `aevum_logic::rust_free_string`.
    fn allocate_c_string(s: &str) -> *mut c_char {
        CString::new(s).unwrap().into_raw()
    }

    // ==================================================================================
    //  FFI INTEGRATION TESTS
    // ==================================================================================

    #[test]
    fn test_ffi_full_query_lifecycle() {
        // 1. SETUP: Marshal Rust strings into C-compatible raw pointers.
        let data =
            allocate_c_string(r#"[{"id": 1, "name": "Ananda"}, {"id": 2, "name": "Aevum"}]"#);
        let query = allocate_c_string(r#"{"id": 1}"#);
        let sort = allocate_c_string("{}");
        let projection = allocate_c_string("{}");

        // 2. EXECUTE: Call the core engine via the FFI boundary.
        // We simulate a Limit of 10 and Skip of 0.
        let result_ptr = aevum_logic::rust_find(data, query, sort, projection, 10, 0);

        // 3. VERIFY: Ensure pointers are valid and logic is correct.
        assert!(
            !result_ptr.is_null(),
            "FFI Critical Failure: Returned NULL pointer."
        );

        let result_str = unsafe { CStr::from_ptr(result_ptr) }
            .to_str()
            .expect("Failed to decode result as UTF-8");

        println!(
            "DEBUG [test_ffi_full_query_lifecycle] Result: {}",
            result_str
        );

        assert!(
            result_str.contains("Ananda"),
            "Query Logic Error: Expected 'Ananda' in result set."
        );
        assert!(
            !result_str.contains("Aevum"),
            "Query Logic Error: Expected 'Aevum' to be filtered out."
        );

        // 4. TEARDOWN: Manually release all heap memory.
        unsafe {
            aevum_logic::rust_free_string(result_ptr);
        }
        unsafe {
            aevum_logic::rust_free_string(data);
        }
        unsafe {
            aevum_logic::rust_free_string(query);
        }
        unsafe {
            aevum_logic::rust_free_string(sort);
        }
        unsafe {
            aevum_logic::rust_free_string(projection);
        }
    }

    #[test]
    fn test_ffi_schema_validation() {
        // 1. SETUP
        let doc = allocate_c_string(r#"{"username": "admin", "role": "root"}"#);
        // Simple schema: just validates that the root is an object.
        let schema = allocate_c_string(r#"{"type": "object"}"#);

        // 2. EXECUTE
        let is_valid = aevum_logic::rust_validate(doc, schema);

        // 3. VERIFY
        assert!(is_valid, "Validation Error: Document should match schema.");

        // 4. TEARDOWN
        unsafe {
            aevum_logic::rust_free_string(doc);
        }
        unsafe {
            aevum_logic::rust_free_string(schema);
        }
    }

    #[test]
    fn test_ffi_aggregation_count() {
        // 1. SETUP
        let data = allocate_c_string(r#"[{"x": 10}, {"x": 20}, {"x": 10}]"#);
        let query = allocate_c_string(r#"{"x": 10}"#);

        // 2. EXECUTE
        let count = aevum_logic::rust_count(data, query);

        // 3. VERIFY
        assert_eq!(count, 2, "Aggregation Error: Expected count of 2.");

        // 4. TEARDOWN
        unsafe {
            aevum_logic::rust_free_string(data);
        }
        unsafe {
            aevum_logic::rust_free_string(query);
        }
    }
}
