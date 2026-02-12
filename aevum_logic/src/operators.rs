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

//! # AevumDB Operator Evaluation Logic
//!
//! This module implements the low-level comparison logic for the AevumDB query engine.
//! It functions as the **Arithmetic Logic Unit (ALU)** of the database, responsible for
//! determining the truthiness of individual field constraints against query predicates.
//!
//! ## Operational Semantics
//!
//! | Operator | Description | Implementation Logic |
//! |----------|-------------|----------------------|
//! | `$eq` | Equality | Strict structural equality (`val == target`) |
//! | `$ne` | Not Equal | Strict structural inequality (`val != target`) |
//! | `$gt` | Greater Than | Numeric comparison (`val > target`) |
//! | `$lt` | Less Than | Numeric comparison (`val < target`) |
//! | `$gte` | Greater Than or Equal | Numeric comparison (`val >= target`) |
//! | `$lte` | Less Than or Equal | Numeric comparison (`val <= target`) |
//!
//! ## Type Handling & Safety
//!
//! - **Structural Equality:** Uses standard JSON equality rules (e.g., objects match if keys/values are identical).
//! - **Numeric Unification:** Range operators (`$gt`, etc.) strictly require **Numeric** types.
//!   AevumDB unifies Integers and Floats into `f64` for comparison. Comparing mismatched types
//!   (e.g., String vs Number) results in `false` (safe failure) rather than a runtime panic.

use serde_json::Value;

// ==================================================================================
//  PUBLIC API
// ==================================================================================

/// Evaluates a single condition against a document field.
///
/// This function acts as a dynamic dispatcher. It identifies the operator string
/// and routes the values to the appropriate comparison logic.
///
/// # Arguments
/// * `op` - The operator key (e.g., `"$gt"`, `"$eq"`).
/// * `field_val` - The value found in the document (the "subject").
/// * `target_val` - The value specified in the query (the "object").
///
/// # Returns
/// * `true` - If the condition is met.
/// * `false` - If the condition is not met, types are incompatible, or the operator is unknown.
///
/// # Behavior on Type Mismatch
/// If an operator requires numeric context (like `$gt`) but receives non-numeric types
/// (like Strings), it returns `false` by default. This ensures the query engine remains
/// robust against dirty data.
pub fn evaluate(op: &str, field_val: &Value, target_val: &Value) -> bool {
    match op {
        // --- Equality Operators ---
        // These operate on all JSON types (Strings, Numbers, Objects, Arrays) via structural equality.
        "$eq" => field_val == target_val,
        "$ne" => field_val != target_val,

        // --- Numeric Comparison Operators ---
        // These strictly require both operands to be coercible to f64.
        "$gt" => compare_f64(field_val, target_val, |a, b| a > b),
        "$lt" => compare_f64(field_val, target_val, |a, b| a < b),
        "$gte" => compare_f64(field_val, target_val, |a, b| a >= b),
        "$lte" => compare_f64(field_val, target_val, |a, b| a <= b),

        // --- Fallback ---
        // Unknown operators are treated as "no match" to prevent undefined behavior.
        _ => false,
    }
}

// ==================================================================================
//  PRIVATE HELPERS
// ==================================================================================

/// Safely performs a numeric comparison between two JSON values.
///
/// Since JSON differentiates between Integers (`i64`/`u64`) and Floats (`f64`),
/// this helper unifies them by casting both sides to `f64` before comparing.
/// This ensures that `10` (Integer) is correctly identified as equal to `10.0` (Float).
///
/// # Performance Note
/// This function is marked `#[inline]` to allow the compiler to optimize away the
/// function call overhead during tight loops (e.g., table scans).
///
/// # Logic
/// 1. Attempt to cast `a` (field value) to `f64`.
/// 2. Attempt to cast `b` (target value) to `f64`.
/// 3. If **both** succeed, execute the comparison closure `op`.
/// 4. If **either** fails (e.g., comparing a String to a Number), return `false`.
///
/// # Arguments
/// * `a` - The first value.
/// * `b` - The second value.
/// * `op` - A closure defining the comparison strategy.
#[inline]
fn compare_f64<F>(a: &Value, b: &Value, op: F) -> bool
where
    F: Fn(f64, f64) -> bool,
{
    // serde_json::Value::as_f64() handles both integer and float variants automatically,
    // providing a unified numeric interface.
    if let (Some(val_a), Some(val_b)) = (a.as_f64(), b.as_f64()) {
        op(val_a, val_b)
    } else {
        // Fail safe: Non-numeric types cannot participate in numeric range comparisons.
        false
    }
}
