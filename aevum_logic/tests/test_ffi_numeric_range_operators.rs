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
    //  TEST HELPERS
    // ==================================================================================

    /// Allocates a C-compatible string on the heap and returns a raw pointer.
    ///
    /// # Memory Safety Warning
    /// This function transfers ownership of the memory to the caller.
    /// The caller is strictly responsible for deallocating this memory using
    /// `aevum_logic::rust_free_string` to prevent memory leaks during testing.
    fn allocate_c_string(s: &str) -> *mut c_char {
        CString::new(s).unwrap().into_raw()
    }

    // ==================================================================================
    //  INTEGRATION TESTS
    // ==================================================================================

    #[test]
    fn test_ffi_numeric_range_operators() {
        // 1. SETUP: Prepare input data simulating a C++ host environment.
        // We use raw pointers here to strictly test the FFI boundary conditions.
        let data = allocate_c_string(r#"[{"id": 1, "age": 20}, {"id": 2, "age": 30}]"#);
        let query_gt = allocate_c_string(r#"{"age": {"$gt": 25}}"#);
        let sort = allocate_c_string("{}");
        let proj = allocate_c_string("{}");

        // 2. EXECUTION: Call the primary entry point `rust_find`.
        let res_ptr = aevum_logic::rust_find(data, query_gt, sort, proj, 10, 0);

        // 3. ASSERTION: Validate the pointer and content.
        assert!(
            !res_ptr.is_null(),
            "FFI Critical Failure: Returned NULL pointer."
        );

        // Convert the result back to a Rust slice for inspection (unsafe dereference).
        let res_str = unsafe { CStr::from_ptr(res_ptr) }
            .to_str()
            .expect("Failed to parse result as UTF-8");

        println!(
            "DEBUG [test_ffi_numeric_range_operators] Output: {}",
            res_str
        );

        // Verify logic: Only age > 25 (ID 2) should remain.
        assert!(
            res_str.contains(r#""id":2"#),
            "Logic Error: Result missing target document (ID 2). Output: {}",
            res_str
        );
        assert!(
            !res_str.contains(r#""id":1"#),
            "Logic Error: Result contains filtered document (ID 1). Output: {}",
            res_str
        );

        // 4. CLEANUP: Manually free all heap allocations to mimic C++ lifecycle.
        aevum_logic::rust_free_string(res_ptr); // Free result
        aevum_logic::rust_free_string(data); // Free inputs
        aevum_logic::rust_free_string(query_gt);
        aevum_logic::rust_free_string(sort);
        aevum_logic::rust_free_string(proj);
    }

    #[test]
    fn test_ffi_exact_equality() {
        // 1. SETUP
        let data = allocate_c_string(r#"[{"name": "Ananda"}, {"name": "Aevum"}]"#);
        let query = allocate_c_string(r#"{"name": "Ananda"}"#);
        let empty_config = allocate_c_string("{}");

        // 2. EXECUTION
        // Reusing `empty_config` for both sort and projection is safe here.
        let res_ptr = aevum_logic::rust_find(data, query, empty_config, empty_config, 10, 0);

        // 3. ASSERTION
        let res_str = unsafe { CStr::from_ptr(res_ptr) }
            .to_str()
            .expect("Invalid UTF-8 sequence in result");

        println!("DEBUG [test_ffi_exact_equality] Output: {}", res_str);

        assert!(
            res_str.contains("Ananda"),
            "Logic Error: Expected 'Ananda' in result."
        );
        assert!(
            !res_str.contains("Aevum"),
            "Logic Error: Expected 'Aevum' to be filtered out."
        );

        // 4. CLEANUP
        aevum_logic::rust_free_string(res_ptr);
        aevum_logic::rust_free_string(data);
        aevum_logic::rust_free_string(query);
        // Note: empty_config was passed twice as const char*, but allocated once.
        // We only free it once.
        aevum_logic::rust_free_string(empty_config);
    }
}
