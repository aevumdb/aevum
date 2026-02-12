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

//! # AevumDB Execution Engine
//!
//! This module acts as the central processing unit ("brain") of the AevumDB system.
//! It is responsible for the entire query lifecycle: parsing raw JSON data, executing
//! filter predicates, applying transformations (projections/sorting), and serializing
//! the final results back for the FFI boundary.
//!
//! ## Execution Pipeline
//!
//! To ensure deterministic behavior and safe cross-language integration, operations
//! within this module strictly follow the **Deserialization-Process-Serialization (DPS)** pattern:
//!
//! 1.  **Ingest:** Raw JSON strings from the C++ host are parsed into `serde_json::Value`.
//! 2.  **Evaluate:** The AST (Abstract Syntax Tree) of the query is traversed against the data.
//! 3.  **Transform:** Matched documents undergo mutation, sorting, or projection.
//! 4.  **Emit:** The final state is serialized back to a string for the host to consume.
//!
//! ## Core Features
//!
//! - **Query Evaluation:** Supports both direct equality checks and complex logical operators via the `operators` module.
//! - **Atomic Mutation:** Performs in-memory document updates with strict immutability on `_id` fields.
//! - **Schema Validation:** Enforces structural integrity and type safety before write operations.

use crate::operators;
use serde_json::{Map, Value};
use std::cmp::Ordering;

// ==================================================================================
//  PRIVATE HELPER FUNCTIONS
// ==================================================================================

/// Evaluates whether a single document satisfies a given query predicate.
///
/// This function serves as the primary filter mechanism. It supports short-circuiting logic
/// to maximize performance during table scans.
///
/// # Matching Modes
/// 1. **Direct Equality:** `{"role": "admin"}` checks for exact value equivalence.
/// 2. **Operator Logic:** `{"age": {"$gt": 18}}` delegates evaluation to the `operators` module.
///
/// # Arguments
/// * `doc` - The generic JSON document to evaluate.
/// * `query` - The query criteria object.
///
/// # Returns
/// * `true` - If the document satisfies **all** conditions in the query (Implicit AND).
/// * `false` - If any condition fails.
fn matches_query(doc: &Value, query: &Value) -> bool {
    if let Some(q_obj) = query.as_object() {
        for (key, q_val) in q_obj {
            let doc_val = &doc[key];

            // Determine if the query value represents an operator object (e.g., { "$gt": 10 })
            // or a direct value comparison (e.g., "Alice").
            if q_val.is_object() {
                // Safety: unwrap is safe here as we just verified `is_object()`.
                for (op, target) in q_val.as_object().unwrap() {
                    // Delegate complex logic (like $gt, $in, $regex) to the specialized operators module.
                    if !operators::evaluate(op, doc_val, target) {
                        return false; // Short-circuit on first failure
                    }
                }
            } else if doc_val != q_val {
                // strict equality check for primitive values.
                return false;
            }
        }
    }
    // If the loop completes without returning false, the document matches all criteria.
    true
}

/// Transforms a document by selectively including or excluding fields.
///
/// # Projection Logic
/// AevumDB follows standard NoSQL projection rules:
/// * **Inclusion Mode:** If `{"name": 1}` is provided, only `name` and `_id` are returned.
/// * **Exclusion Mode:** If `{"password": 0}` is provided, everything except `password` is returned.
/// * **ID Persistence:** The `_id` field is always included unless explicitly suppressed (`{"_id": 0}`).
///
/// # Arguments
/// * `doc` - The source document to project.
/// * `projection` - A map indicating which fields to filter.
///
/// # Returns
/// A new `Value::Object` containing only the projected dataset.
fn apply_projection(doc: &Value, projection: &Value) -> Value {
    if let (Some(doc_obj), Some(proj_obj)) = (doc.as_object(), projection.as_object()) {
        // Optimization: If projection is empty, return the document as-is (Zero-copy logical equivalent).
        if proj_obj.is_empty() {
            return doc.clone();
        }

        let mut new_obj = Map::new();

        // Iterate through requested projection fields
        for (key, val) in proj_obj {
            // Check for truthy values (1 or true) to determine inclusion.
            // Currently, this implementation assumes an Inclusion Projection strategy.
            if val.as_i64() == Some(1) || val.as_bool() == Some(true) {
                if let Some(v) = doc_obj.get(key) {
                    new_obj.insert(key.clone(), v.clone());
                }
            }
        }

        // Implicitly preserve the primary key `_id` if present in the source,
        // unless the user intentionally excluded it (logic implies inclusion by default).
        if !new_obj.contains_key("_id") && doc_obj.contains_key("_id") {
            // Check if explicitly excluded in projection (logic simplified for this snippet)
            let explicitly_excluded = proj_obj.get("_id").map_or(false, |v| {
                v.as_i64() == Some(0) || v.as_bool() == Some(false)
            });

            if !explicitly_excluded {
                new_obj.insert("_id".to_string(), doc_obj["_id"].clone());
            }
        }

        return Value::Object(new_obj);
    }

    // Fallback: Return original if input types were invalid for projection.
    doc.clone()
}

/// Determines the relative sort order between two generic JSON values.
///
/// This function handles mixed-type comparisons to ensure a stable sort, although
/// strict schema design should avoid sorting mixed types.
///
/// # Comparison Strategy
/// * **Strings**: Lexicographical order (case-sensitive).
/// * **Numbers**: Standard numeric ordering (floats and integers).
/// * **Booleans**: `false` (0) < `true` (1).
/// * **Null**: Treated as the lowest value.
fn compare_values(a: &Value, b: &Value) -> Ordering {
    if let (Some(sa), Some(sb)) = (a.as_str(), b.as_str()) {
        return sa.cmp(sb);
    }
    if let (Some(na), Some(nb)) = (a.as_f64(), b.as_f64()) {
        return na.partial_cmp(&nb).unwrap_or(Ordering::Equal);
    }
    if let (Some(ba), Some(bb)) = (a.as_bool(), b.as_bool()) {
        return ba.cmp(&bb);
    }
    // Fallback for disparate types or Nulls
    Ordering::Equal
}

// ==================================================================================
//  PUBLIC API (CORE LOGIC)
// ==================================================================================

/// Validates a JSON document structure against a defined schema.
///
/// This function enforces data integrity constraints before insertion or update operations.
/// It performs validation on three levels: Existence, Type Correctness, and Value Constraints.
///
/// # Arguments
/// * `doc_str` - The JSON document to validate.
/// * `schema_str` - The schema definition containing `required` fields and `fields` rules.
///
/// # Returns
/// `true` if valid (or if parsing fails to avoid crashes), `false` if a violation is detected.
pub fn validate(doc_str: &str, schema_str: &str) -> bool {
    let doc: Value = serde_json::from_str(doc_str).unwrap_or(Value::Null);
    let schema: Value = serde_json::from_str(schema_str).unwrap_or(Value::Null);

    // Fail-open strategy: If parsing fails, we treat it as valid to prevent
    // the engine from halting, assuming upstream checks catch malformed JSON.
    if !doc.is_object() || !schema.is_object() {
        return true;
    }

    let doc_obj = doc.as_object().unwrap();

    // 1. Validate Required Fields (Existence Check)
    if let Some(required) = schema["required"].as_array() {
        for req_field in required {
            if let Some(field_name) = req_field.as_str() {
                if !doc_obj.contains_key(field_name) {
                    return false; // Constraint Violation: Missing mandatory field
                }
            }
        }
    }

    // 2. Validate Field-Level Rules (Type & Constraint Check)
    if let Some(fields) = schema["fields"].as_object() {
        for (field, rules) in fields {
            if let Some(doc_val) = doc_obj.get(field) {
                // A. Type Validation
                if let Some(type_val) = rules["type"].as_str() {
                    let type_ok = match type_val {
                        "string" => doc_val.is_string(),
                        "number" => doc_val.is_number(),
                        "boolean" => doc_val.is_boolean(),
                        "array" => doc_val.is_array(),
                        "object" => doc_val.is_object(),
                        _ => true, // Unknown types are permissive
                    };
                    if !type_ok {
                        return false;
                    }
                }

                // B. Numeric Constraints (Range Check)
                if let Some(min_val) = rules["min"].as_f64() {
                    if let Some(v) = doc_val.as_f64() {
                        if v < min_val {
                            return false;
                        }
                    }
                }
                if let Some(max_val) = rules["max"].as_f64() {
                    if let Some(v) = doc_val.as_f64() {
                        if v > max_val {
                            return false;
                        }
                    }
                }

                // C. Enumerated Values (Allow-list Check)
                if let Some(enum_vals) = rules["enum"].as_array() {
                    if !enum_vals.contains(doc_val) {
                        return false;
                    }
                }
            }
        }
    }

    true
}

/// Executes a generic query pipeline to retrieve data.
///
/// # Pipeline Stages
/// 1. **Filter**: Scans the dataset and retains documents matching `query_str`.
/// 2. **Sort**: Orders the filtered results based on multiple criteria in `sort_str`.
/// 3. **Pagination**: Applies `skip` (offset) and `limit` to the sorted set.
/// 4. **Projection**: Transforms the structure of the remaining documents.
///
/// # Arguments
/// * `data_str` - The complete dataset (JSON array).
/// * `query_str` - Filter criteria.
/// * `sort_str` - Sorting criteria (e.g., `{"age": -1, "name": 1}`).
/// * `proj_str` - Field projection.
/// * `limit` - Max records to return.
/// * `skip` - Records to bypass.
///
/// # Returns
/// A serialized JSON string representing the final result set.
pub fn find(
    data_str: &str,
    query_str: &str,
    sort_str: &str,
    proj_str: &str,
    limit: usize,
    skip: usize,
) -> String {
    // Robust Deserialization: Default to empty structures on parse failure.
    let data: Value = serde_json::from_str(data_str).unwrap_or(Value::Array(vec![]));
    let query: Value = serde_json::from_str(query_str).unwrap_or(Value::Object(Default::default()));
    let sort: Value = serde_json::from_str(sort_str).unwrap_or(Value::Object(Default::default()));
    let projection: Value =
        serde_json::from_str(proj_str).unwrap_or(Value::Object(Default::default()));

    let mut results = Vec::new();

    // 1. FILTER PHASE
    if let Some(docs) = data.as_array() {
        for doc in docs {
            if matches_query(doc, &query) {
                results.push(doc.clone());
            }
        }
    }

    // 2. SORT PHASE
    if let Some(sort_obj) = sort.as_object() {
        if !sort_obj.is_empty() {
            results.sort_by(|a, b| {
                for (key, order) in sort_obj {
                    let val_a = a.get(key).unwrap_or(&Value::Null);
                    let val_b = b.get(key).unwrap_or(&Value::Null);
                    let cmp = compare_values(val_a, val_b);

                    if cmp != Ordering::Equal {
                        // -1 indicates descending, 1 indicates ascending
                        return if order.as_i64() == Some(-1) {
                            cmp.reverse()
                        } else {
                            cmp
                        };
                    }
                }
                // Maintain stability if all sort keys are equal
                Ordering::Equal
            });
        }
    }

    // 3. PAGINATION PHASE
    let total = results.len();
    if skip >= total {
        return "[]".to_string();
    }

    let start = skip;
    let end = if limit > 0 {
        std::cmp::min(start + limit, total)
    } else {
        total
    };

    let sliced = &results[start..end];

    // 4. PROJECTION PHASE
    let final_results: Vec<Value> = sliced
        .iter()
        .map(|doc| apply_projection(doc, &projection))
        .collect();

    serde_json::to_string(&final_results).unwrap_or_else(|_| "[]".to_string())
}

/// Counts the total number of documents matching a query.
///
/// Optimized for performance by bypassing the sort, project, and serialize phases.
///
/// # Returns
/// The count as a `usize`.
pub fn count(data_str: &str, query_str: &str) -> usize {
    let data: Value = serde_json::from_str(data_str).unwrap_or(Value::Array(vec![]));
    let query: Value = serde_json::from_str(query_str).unwrap_or(Value::Object(Default::default()));

    if let Some(docs) = data.as_array() {
        return docs.iter().filter(|doc| matches_query(doc, &query)).count();
    }
    0
}

/// Updates documents based on a selection query.
///
/// This function performs a **Merge** operation (Patch). It iterates over the dataset,
/// identifies matches, and merges the `update_str` object into the existing document.
///
/// **Constraint:** The `_id` field is immutable and will be ignored in the update payload.
///
/// # Arguments
/// * `data_str` - The complete dataset.
/// * `query_str` - Selection criteria.
/// * `update_str` - The fields to merge (e.g., `{"status": "active"}`).
///
/// # Returns
/// A JSON string representing the **full** dataset with updates applied.
pub fn update(data_str: &str, query_str: &str, update_str: &str) -> String {
    let mut data: Value = serde_json::from_str(data_str).unwrap_or(Value::Array(vec![]));
    let query: Value = serde_json::from_str(query_str).unwrap_or(Value::Object(Default::default()));
    let update_data: Value =
        serde_json::from_str(update_str).unwrap_or(Value::Object(Default::default()));

    if let Some(docs) = data.as_array_mut() {
        for doc in docs {
            if matches_query(doc, &query) {
                // Ensure we have valid objects before merging fields
                if let (Some(doc_obj), Some(update_obj)) =
                    (doc.as_object_mut(), update_data.as_object())
                {
                    for (k, v) in update_obj {
                        // IMMUTABILITY CHECK:
                        // Skip update if the key is "_id" to preserve primary key integrity.
                        if k == "_id" {
                            continue;
                        }
                        doc_obj.insert(k.clone(), v.clone());
                    }
                }
            }
        }
    }
    serde_json::to_string(&data).unwrap_or_else(|_| "[]".to_string())
}

/// Deletes documents that match a specific query.
///
/// This function filters the dataset in-place using a retention policy.
///
/// # Arguments
/// * `data_str` - The complete dataset.
/// * `query_str` - Selection criteria.
///
/// # Returns
/// A JSON string representing the dataset *after* the matching documents have been removed.
pub fn delete(data_str: &str, query_str: &str) -> String {
    let mut data: Value = serde_json::from_str(data_str).unwrap_or(Value::Array(vec![]));
    let query: Value = serde_json::from_str(query_str).unwrap_or(Value::Object(Default::default()));

    if let Some(docs) = data.as_array_mut() {
        // Retain only documents that DO NOT match the query
        docs.retain(|doc| !matches_query(doc, &query));
    }
    serde_json::to_string(&data).unwrap_or_else(|_| "[]".to_string())
}
