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
 * @file logger.hpp
 * @brief Thread-safe diagnostic logging facility for AevumDB.
 *
 * @details
 * This header declares the `Logger` class, which serves as the centralized
 * reporting interface for the database engine. It guarantees atomic output
 * to standard streams (`stdout`/`stderr`) across concurrent execution threads,
 * preventing message interleaving and ensuring log integrity.
 */

#pragma once

#include <mutex>
#include <string>

namespace aevum::infra {

/**
 * @enum LogLevel
 * @brief Defines the severity hierarchy for diagnostic messages.
 *
 * Used to categorize the criticality of log entries and determine the
 * appropriate output stream (Standard Output vs. Standard Error).
 */
enum class LogLevel {
    TRACE, ///< Granular execution flow details (e.g., loop iterations, variable dumps).
    DEBUG, ///< Diagnostic information intended for development and troubleshooting.
    INFO,  ///< Nominal operational events (e.g., startup sequence, status heartbeats).
    WARN,  ///< Non-blocking anomalies or potential misconfigurations.
    ERROR, ///< Recoverable runtime errors that do not halt the system.
    FATAL  ///< Critical system failures requiring immediate process termination.
};

/**
 * @class Logger
 * @brief A static utility class providing system-wide logging capabilities.
 *
 * @details
 * The Logger implements a thread-safe, static interface for writing diagnostic
 * artifacts. It utilizes an internal mutex to serialize access to the console,
 * ensuring that log entries from multiple worker threads remain distinct and readable.
 */
class Logger {
  public:
    /**
     * @brief Writes a formatted diagnostic message to the console.
     *
     * The output includes a high-precision timestamp, the severity tag, and the payload.
     *
     * **Stream Routing Logic:**
     * - `TRACE`, `DEBUG`, `INFO`: Routed to `std::cout` (Standard Output).
     * - `WARN`, `ERROR`, `FATAL`: Routed to `std::cerr` (Standard Error) for immediate flushing.
     *
     * @param level The severity classification of the message.
     * @param message The content payload to be logged.
     *
     * @note This function is thread-safe and blocking. It acquires a static lock
     * before writing to the stream.
     *
     * @code
     * // Example Usage:
     * aevum::infra::Logger::log(LogLevel::INFO, "Storage engine initialized.");
     * aevum::infra::Logger::log(LogLevel::ERROR, "IO Error: Disk quota exceeded.");
     * @endcode
     */
    static void log(LogLevel level, const std::string& message);

  private:
    /**
     * @brief Global synchronization primitive.
     *
     * Guards access to `std::cout` and `std::cerr` to prevent race conditions
     * (interleaved text) when multiple threads log simultaneously.
     */
    static std::mutex mutex_;
};

} // namespace aevum::infra
