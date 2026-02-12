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
 * @file string.cpp
 * @brief Implementation of high-performance string manipulation primitives.
 *
 * @details
 * This file contains the concrete implementation of the static methods defined
 * in the `String` class. It employs efficient bidirectional iterator scanning
 * to perform low-level character manipulation with minimal memory overhead.
 */

#include "aevum/infra/string.hpp"

#include <algorithm>
#include <cctype>

namespace aevum::infra {

/**
 * @brief Trims leading and trailing whitespace from a string instance.
 *
 * Implementation Strategy:
 * 1. **Linear Prefix Scan**: Identifies the first non-whitespace character using
 * the standard locale's predicate.
 * 2. **Empty State Detection**: Provides an early exit if the string is
 * composed entirely of whitespace characters.
 * 3. **Linear Suffix Scan**: Identifies the terminal non-whitespace character
 * by scanning backwards from the end of the buffer.
 * 4. **Range Construction**: Substrings the valid range into a new `std::string`.
 *
 * @note The use of `static_cast<unsigned char>` is critical to prevent undefined
 * behavior with `std::isspace` when encountering characters with negative values
 * in signed `char` environments.
 *
 * @param s The source string to be sanitized.
 * @return std::string The resulting string after stripping whitespace.
 */
std::string String::trim(const std::string& s)
{
    // 1. Prefix Scan: Locate the first character that is NOT a whitespace.
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) {
        start++;
    }

    // 2. Short-circuit: If the buffer is empty or exclusively whitespace,
    // return an empty string immediately.
    if (start == s.end()) {
        return "";
    }

    // 3. Suffix Scan: Locate the final character that is NOT a whitespace.
    // Starting from the terminal iterator and decrementing.
    auto end = s.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(static_cast<unsigned char>(*end)));

    /* * 4. Allocation: Construct and return the sanitized string.
     * The iterator range is defined as [start, end + 1) to ensure the
     * inclusive terminal character is captured by the string constructor.
     */
    return std::string(start, end + 1);
}

} // namespace aevum::infra
