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
 * @file id_generator.hpp
 * @brief Infrastructure utility for generating Universally Unique Identifiers (UUIDs).
 *
 * @details
 * This file declares the `IdGenerator` class, a stateless utility designed to produce
 * collision-resistant, Version 4 (random) UUIDs. It serves as the primary mechanism
 * for assigning unique primary keys (`_id`) to documents within the AevumDB storage engine.
 */

#pragma once

#include <string>

namespace aevum::infra {

/**
 * @class IdGenerator
 * @brief A static utility for generating standard Version 4 UUIDs.
 *
 * @details
 * This class encapsulates the system's entropy source (Random Number Generator)
 * and formatting logic required to produce standard 36-character UUID strings.
 * It is designed to be thread-safe and lightweight.
 */
class IdGenerator {
  public:
    /**
     * @brief Generates a random Version 4 UUID string.
     *
     * The output string adheres to the standard canonical textual representation:
     * `xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx`
     *
     * **Format Specifications:**
     * - `x`: A random hexadecimal digit (0-9, a-f).
     * - `4`: The version identifier (Version 4, Random).
     * - `y`: The variant identifier (Variant 1), strictly limited to `{8, 9, a, b}`.
     *
     * @return std::string The generated UUID (e.g., "123e4567-e89b-12d3-a456-426614174000").
     *
     * @code
     * // Example Usage:
     * std::string doc_id = aevum::infra::IdGenerator::generate();
     * @endcode
     */
    static std::string generate();
};

} // namespace aevum::infra
