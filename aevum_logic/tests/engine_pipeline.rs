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
    //  TEST UTILITIES
    // ==================================================================================

    /// Simulates host-side memory allocation.
    ///
    /// Creates a raw C-string pointer. The caller accepts full ownership and
    /// responsibility for deallocation via `rust_free_string`.
    fn allocate_c_string(s: &str) -> *mut c_char {
        CString::new(s).unwrap().into_raw()
    }

    // ==================================================================================
    //  PIPELINE INTEGRATION TESTS
    // ==================================================================================

    #[test]
    fn test_pipeline_projection_semantics() {
        // SETUP: Define a document with mixed visibility fields.
        let data =
            allocate_c_string(r#"[{"_id": "row1", "public": "visible", "secret": "hidden"}]"#);
        let query_all = allocate_c_string("{}"); // Select All
        let sort_none = allocate_c_string("{}"); // No Sort

        // ------------------------------------------------------------------
        // SCENARIO 1: Standard Inclusion
        // Goal: Retrieve 'public' field. '_id' should be included implicitly.
        // ------------------------------------------------------------------
        let proj_include = allocate_c_string(r#"{ "public": 1 }"#);

        let res_ptr_1 = aevum_logic::rust_find(data, query_all, sort_none, proj_include, 10, 0);
        let res_str_1 = unsafe { CStr::from_ptr(res_ptr_1) }.to_str().unwrap();

        assert!(
            res_str_1.contains("visible"),
            "Projection Error: Missing requested field."
        );
        assert!(
            res_str_1.contains("_id"),
            "Projection Error: '_id' should be implicit."
        );
        assert!(
            !res_str_1.contains("hidden"),
            "Security Error: 'secret' field leaked."
        );

        unsafe {
            aevum_logic::rust_free_string(res_ptr_1);
        }
        unsafe {
            aevum_logic::rust_free_string(proj_include);
        }

        // ------------------------------------------------------------------
        // SCENARIO 2: Explicit Exclusion
        // Goal: Retrieve 'public' field but force suppress '_id'.
        // ------------------------------------------------------------------
        let proj_suppress_id = allocate_c_string(r#"{ "public": 1, "_id": 0 }"#);

        let res_ptr_2 = aevum_logic::rust_find(data, query_all, sort_none, proj_suppress_id, 10, 0);
        let res_str_2 = unsafe { CStr::from_ptr(res_ptr_2) }.to_str().unwrap();

        assert!(res_str_2.contains("visible"));
        assert!(
            !res_str_2.contains("_id"),
            "Projection Error: '_id' should be suppressed."
        );

        // CLEANUP
        unsafe {
            aevum_logic::rust_free_string(res_ptr_2);
        }
        unsafe {
            aevum_logic::rust_free_string(proj_suppress_id);
        }
        unsafe {
            aevum_logic::rust_free_string(data);
        }
        unsafe {
            aevum_logic::rust_free_string(query_all);
        }
        unsafe {
            aevum_logic::rust_free_string(sort_none);
        }
    }

    #[test]
    fn test_pipeline_sorting_determinism() {
        // SETUP: Unsorted data
        let data = allocate_c_string(
            r#"[
            {"id": 1, "score": 50}, 
            {"id": 2, "score": 100}, 
            {"id": 3, "score": 75}
        ]"#,
        );
        let query = allocate_c_string("{}");
        let proj = allocate_c_string("{}");

        // TEST: Sort by 'score' Ascending (1)
        let sort_asc = allocate_c_string(r#"{ "score": 1 }"#);

        let res_ptr = aevum_logic::rust_find(data, query, sort_asc, proj, 10, 0);
        let res_str = unsafe { CStr::from_ptr(res_ptr) }.to_str().unwrap();

        // VERIFICATION: Check the relative position of values in the JSON string
        let idx_50 = res_str.find("50").expect("Missing value 50");
        let idx_75 = res_str.find("75").expect("Missing value 75");
        let idx_100 = res_str.find("100").expect("Missing value 100");

        assert!(idx_50 < idx_75, "Sorting Error: 50 should come before 75");
        assert!(idx_75 < idx_100, "Sorting Error: 75 should come before 100");

        // CLEANUP
        unsafe {
            aevum_logic::rust_free_string(res_ptr);
        }
        unsafe {
            aevum_logic::rust_free_string(sort_asc);
        }
        unsafe {
            aevum_logic::rust_free_string(data);
        }
        unsafe {
            aevum_logic::rust_free_string(query);
        }
        unsafe {
            aevum_logic::rust_free_string(proj);
        }
    }

    #[test]
    fn test_pipeline_integrity_enforcement() {
        // Schema requires the 'pid' (Product ID) field.
        let schema = allocate_c_string(r#"{ "required": ["pid"] }"#);

        // CASE 1: Valid Document
        let doc_valid = allocate_c_string(r#"{ "pid": 101, "name": "Engine" }"#);
        assert!(
            aevum_logic::rust_validate(doc_valid, schema),
            "Validation Error: Valid document was rejected."
        );

        // CASE 2: Invalid Document (Missing 'pid')
        let doc_invalid = allocate_c_string(r#"{ "name": "Orphan Component" }"#);

        // NOTE: Based on 'engine.rs', explicit validation logic DOES return false
        // if a required field is missing.
        let is_valid = aevum_logic::rust_validate(doc_invalid, schema);
        assert!(
            !is_valid,
            "Integrity Error: Engine failed to catch missing required field."
        );

        // CLEANUP
        unsafe {
            aevum_logic::rust_free_string(schema);
        }
        unsafe {
            aevum_logic::rust_free_string(doc_valid);
        }
        unsafe {
            aevum_logic::rust_free_string(doc_invalid);
        }
    }
}
