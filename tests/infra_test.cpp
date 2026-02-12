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
 * @file infra_test.cpp
 * @brief Unit tests for shared infrastructure primitives (IdGenerator, String).
 *
 * @details
 * This file validates the integrity of the foundational utilities in AevumDB.
 * Ensuring the correctness of ID generation and string processing is critical
 * for maintaining primary key uniqueness and query parsing reliability.
 */

#include "aevum/infra/id_generator.hpp"
#include "aevum/infra/string.hpp"
#include "framework.hpp"

#include <string>

/**
 * @brief Validates RFC 4122 compliance for UUID character count.
 *
 * A standard Version 4 UUID must represent 128 bits of data as 32 hexadecimal
 * characters and 4 hyphens, resulting in a fixed length of 36 characters.
 * *
 */
void test_uuid_length()
{
    std::string id = aevum::infra::IdGenerator::generate();
    // Canonical format: 8-4-4-4-12 = 36 characters.
    ASSERT_EQ(id.length(), static_cast<size_t>(36));
}

/**
 * @brief Verifies temporal uniqueness and RNG state progression.
 *
 * Ensures that sequential invocations of the generator produce non-colliding
 * identifiers. This confirms that the thread-local Mersenne Twister engine
 * is advancing its internal state correctly.
 */
void test_uuid_uniqueness()
{
    std::string id1 = aevum::infra::IdGenerator::generate();
    std::string id2 = aevum::infra::IdGenerator::generate();
    ASSERT_NE(id1, id2);
}

/**
 * @brief Tests the `String::trim` algorithm with nominal input.
 *
 * Scenarios verified:
 * - Elimination of leading/trailing space characters.
 * - Integrity of internal whitespace (space preservation between lexemes).
 */
void test_string_trim()
{
    std::string dirty = "   hello aevumdb   ";
    std::string clean = aevum::infra::String::trim(dirty);

    // Verify result is stripped of padding but preserves internal structure.
    ASSERT_EQ(clean, std::string("hello aevumdb"));
}

/**
 * @brief Validates edge case handling for high-entropy whitespace strings.
 *
 * Ensures that strings composed entirely of escape sequences (tabs, newlines)
 * are successfully collapsed into an empty string object.
 */
void test_string_trim_empty()
{
    std::string empty = "  \t\n  \r ";
    std::string result = aevum::infra::String::trim(empty);

    // Assert that the resulting object has zero length.
    ASSERT_EQ(result, std::string(""));
    ASSERT_EQ(result.length(), static_cast<size_t>(0));
}
