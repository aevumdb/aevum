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
 * @file logger.cpp
 * @brief Implementation of the thread-safe diagnostic logging utility.
 *
 * @details
 * This file provides the concrete implementation of the `Logger` class.
 * It manages the atomic orchestration of log message formatting, including
 * ISO 8601-like timestamps, severity tagging, and ANSI color-coded output
 * for enhanced terminal diagnostics.
 */

#include "aevum/infra/logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

namespace aevum::infra {

// Define and initialize the static synchronization primitive.
std::mutex Logger::mutex_;

/**
 * @brief Dispatches a formatted log entry to the appropriate system stream.
 *
 * Operational Logic:
 * 1. **Synchronization**: Acquires a `lock_guard` to prevent interleaved output.
 * 2. **Chronometry**: Captures the current system clock and formats it.
 * 3. **Stream Segregation**: Routes messages to `stdout` or `stderr` based on severity.
 * 4. **Stylization**: Injects ANSI escape sequences for visual categorization.
 */
void Logger::log(LogLevel level, const std::string& message)
{
    // Ensure atomicity of the entire logging operation across concurrent threads.
    std::lock_guard<std::mutex> lock(mutex_);

    // Capture system time for precise event sequencing.
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    // Select target stream based on the criticality of the event.
    // High-priority events (WARN and above) bypass stdout to avoid buffering delays.
    auto& stream = (level >= LogLevel::WARN) ? std::cerr : std::cout;

    // Formatting: [YYYY-MM-DD HH:MM:SS]
    // Note: Mutex protects std::localtime's internal static buffer.
    stream << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << "] ";

    // Apply ANSI escape sequences for severity color-coding.
    switch (level) {
    case LogLevel::TRACE:
        // Gray (Dimmed) - Lowest priority execution data.
        stream << "\033[90m[TRCE] ";
        break;
    case LogLevel::DEBUG:
        // Cyan - Internal state and variable tracking.
        stream << "\033[36m[DBUG] ";
        break;
    case LogLevel::INFO:
        // Green - Nominal operational status.
        stream << "\033[32m[INFO] ";
        break;
    case LogLevel::WARN:
        // Yellow - Non-fatal anomalies or configuration alerts.
        stream << "\033[33m[WARN] ";
        break;
    case LogLevel::ERROR:
        // Red - Failure in non-critical execution paths.
        stream << "\033[31m[FAIL] ";
        break;
    case LogLevel::FATAL:
        // Bold Red - Terminal system failures.
        stream << "\033[1;31m[CRIT] ";
        break;
    }

    // Append payload, reset terminal style, and force stream flush.
    stream << message << "\033[0m" << std::endl;
}

} // namespace aevum::infra
