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
 * @file id_generator.cpp
 * @brief Implementation of the UUID generation utility.
 *
 * @details
 * This file contains the concrete implementation of the `IdGenerator::generate` method.
 * It strictly follows RFC 4122 for Version 4 (Random) UUIDs to ensure high entropy
 * and collision resistance within the AevumDB storage engine.
 */

#include "aevum/infra/id_generator.hpp"

#include <iomanip>
#include <random>
#include <sstream>

namespace aevum::infra {

/**
 * @brief Generates an RFC 4122 compliant Version 4 UUID.
 *
 * Implementation Strategy:
 * 1. **Thread Safety**: Employs `thread_local` random engines to eliminate lock
 * contention and ensure high-performance concurrent ID generation.
 * 2. **Entropy Source**: Seeds a 64-bit Mersenne Twister (MT19937_64) using
 * `std::random_device` for cryptographic-quality randomness.
 * 3. **Protocol Compliance**:
 * - Sets Version bits to `0100` (Version 4) in the 7th byte.
 * - Sets Variant bits to `10` (Variant 1) in the 9th byte.
 *
 * @return std::string A canonically formatted UUID string.
 */
std::string IdGenerator::generate()
{
    // Use thread_local to ensure each thread has its own isolated generator instance.
    // This allows lock-free execution in high-concurrency database environments.
    static thread_local std::random_device rd;
    static thread_local std::mt19937_64 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dis;

    // Generate 128 bits of randomness using two 64-bit samples.
    uint64_t p1 = dis(gen);
    uint64_t p2 = dis(gen);

    std::stringstream ss;
    ss << std::hex
       << std::setfill('0')

       // Group 1: Time-low (8 hex digits / 32 bits)
       << std::setw(8) << static_cast<uint32_t>(p1 >> 32)
       << "-"

       // Group 2: Time-mid (4 hex digits / 16 bits)
       << std::setw(4) << static_cast<uint16_t>((p1 >> 16) & 0xFFFF)
       << "-"

       // Group 3: Time-high and Version (4 hex digits / 16 bits)
       // Force high nibble to '4' (Version 4: Random).
       << std::setw(4) << ((p1 & 0x0FFF) | 0x4000)
       << "-"

       // Group 4: Clock-seq-and-reserved and Clock-seq-low (4 hex digits / 16 bits)
       // Force high bits to '10' (Variant 1: RFC 4122).
       << std::setw(4) << (((p2 >> 48) & 0x3FFF) | 0x8000)
       << "-"

       // Group 5: Node ID (12 hex digits / 48 bits)
       << std::setw(12) << (p2 & 0xFFFFFFFFFFFF);

    return ss.str();
}

} // namespace aevum::infra
