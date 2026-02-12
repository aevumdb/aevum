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
 * @file ffi_test.cpp
 * @brief Integration tests for the C++/Rust Foreign Function Interface (FFI).
 *
 * @details
 * These tests validate the integrity of the bridge between the C++ host and
 * the Rust execution engine. It ensures that JSON payloads are correctly
 * marshaled across the ABI boundary and that the Rust core returns
 * deterministic results.
 */

#include "aevum/ffi/rust_adapter.hpp"
#include "framework.hpp"

#include <cJSON.h>
#include <string>

/**
 * @brief Tests the `find` execution pipeline via FFI.
 * * Verifies that the Rust engine correctly evaluates complex query predicates
 * (e.g., $gt) and returns the filtered dataset to C++.
 */
void test_rust_find_logic()
{
    std::string data = "[{\"val\": 10}, {\"val\": 50}]";
    std::string query = "{\"val\": {\"$gt\": 20}}";
    std::string sort = "{}";
    std::string proj = "{}";

    // Invoke the Rust logic via the Aevum FFI adapter
    std::string res = aevum::ffi::call_find(data, query, sort, proj, 0, 0);

    // Parse the returned serialized JSON
    cJSON* json = cJSON_Parse(res.c_str());

    // Safety Assertion: Ensure the Rust engine returned valid, parseable JSON.
    ASSERT_NE(json, (cJSON*)nullptr);

    // Logic Assertion: Only the document with val=50 satisfies 'val > 20'.
    ASSERT_EQ(cJSON_GetArraySize(json), 1);

    cJSON* item = cJSON_GetArrayItem(json, 0);
    cJSON* val = cJSON_GetObjectItem(item, "val");
    ASSERT_EQ((int)val->valuedouble, 50);

    cJSON_Delete(json);
}

/**
 * @brief Tests the `update` mutation logic via FFI.
 * * Ensures that the Rust engine can perform in-memory document modification
 * and return the full updated state back to the C++ host.
 */
void test_rust_update_logic()
{
    std::string data = "[{\"id\": 1, \"v\": 10}]";
    std::string query = "{\"id\": 1}";
    std::string update = "{\"v\": 99}";

    // Execute the FFI mutation call
    std::string res = aevum::ffi::call_update(data, query, update);

    cJSON* json = cJSON_Parse(res.c_str());

    // Safety Assertion
    ASSERT_NE(json, (cJSON*)nullptr);

    cJSON* item = cJSON_GetArrayItem(json, 0);
    cJSON* val = cJSON_GetObjectItem(item, "v");

    // Verify the value was successfully updated to 99 in the result set.
    ASSERT_EQ((int)val->valuedouble, 99);

    cJSON_Delete(json);
}

/**
 * @brief Tests the `validate` constraint engine via FFI.
 * * Validates the schema enforcement logic including type checking,
 * range constraints, and mandatory field existence.
 */
void test_rust_validate_logic()
{
    // Define a strict schema with requirements and numeric constraints
    std::string schema =
        "{\"required\": [\"name\"], \"fields\": {\"age\": {\"type\": \"number\", \"min\": 18}}}";

    // Scenario 1: Nominal Success (Valid types and values)
    std::string valid_doc = "{\"name\": \"adult\", \"age\": 20}";
    ASSERT_TRUE(aevum::ffi::call_validate(valid_doc, schema));

    // Scenario 2: Semantic Violation (Numeric value below 'min' constraint)
    std::string invalid_doc = "{\"name\": \"kid\", \"age\": 10}";
    ASSERT_FALSE(aevum::ffi::call_validate(invalid_doc, schema));

    // Scenario 3: Structural Violation (Missing 'required' field)
    std::string missing_field = "{\"age\": 25}";
    ASSERT_FALSE(aevum::ffi::call_validate(missing_field, schema));
}
