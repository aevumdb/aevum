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
 * @file string.hpp
 * @brief Supplementary string manipulation primitives.
 *
 * @details
 * This header defines the `String` utility class, acting as a static extension
 * to the C++ standard library's `std::string`. It provides high-frequency
 * text processing algorithms (such as trimming) optimized for the AevumDB
 * query parser and storage engine.
 */

#pragma once

#include <string>

namespace aevum::infra {

/**
 * @class String
 * @brief A static container for text processing algorithms.
 *
 * @details
 * This class functions as a namespace wrapper for stateless string operations.
 * It is designed to handle common string sanitization tasks that are not
 * directly exposed by the standard `std::string` API.
 */
class String {
  public:
    /**
     * @brief Trims leading and trailing whitespace from a string.
     *
     * Performs a bidirectional scan to strip characters classified as whitespace
     * by the standard locale. This is critical for sanitizing user inputs like
     * SQL-like queries or configuration values.
     *
     * **Whitespace Definitions:**
     * - Space (`0x20`)
     * - Horizontal Tab (`\t`)
     * - Newline (`\n`)
     * - Carriage Return (`\r`)
     * - Vertical Tab (`\v`)
     * - Form Feed (`\f`)
     *
     * @param s The source string to process.
     * @return std::string A new string instance containing the trimmed content.
     * Returns an empty string if the input is empty or consists solely of whitespace.
     *
     * @code
     * // Example Usage:
     * std::string raw = "   SELECT * FROM users   \n";
     * std::string clean = aevum::infra::String::trim(raw); // "SELECT * FROM users"
     * @endcode
     */
    static std::string trim(const std::string& s);
};

} // namespace aevum::infra
