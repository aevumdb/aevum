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
 * @file engine.cpp
 * @brief Implementation of the file persistence layer.
 *
 * @details
 * This file implements the Append-Only Log (AOL) storage mechanism.
 * Unlike traditional text logs, AevumDB uses a **Binary Length-Prefixed Frame** format:
 * `[4-byte Little Endian Length Header] + [N-byte UTF-8 Payload]`
 *
 * This ensures strict data boundaries, enabling robust crash recovery and
 * high-throughput sequential I/O.
 */

#include "aevum/storage/engine.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

// Alias for standard filesystem namespace to keep code concise and readable.
namespace fs = std::filesystem;

namespace aevum::storage {

/**
 * @brief Constructs the storage engine configuration.
 * @param base_path The root directory path for the database files.
 */
Engine::Engine(std::string base_path) : base_path_(std::move(base_path)) {}

/**
 * @brief Initializes the storage subsystem.
 *
 * Bootstraps the environment by verifying the existence of the data root.
 * Creates the directory tree recursively if it does not exist.
 */
void Engine::init()
{
    if (!fs::exists(base_path_)) {
        fs::create_directories(base_path_);
    }
}

/**
 * @brief Resolves the full filesystem path for a collection's artifact.
 *
 * @param collection The logical collection name.
 * @return std::string The absolute path with the `.aev` binary extension.
 */
std::string Engine::get_path(const std::string& collection)
{
    // The .aev extension signifies the binary log format used by AevumDB.
    return base_path_ + "/" + collection + ".aev";
}

/**
 * @brief Scans the storage directory for existing database artifacts.
 *
 * Iterates through the filesystem to identify valid AevumDB collection files.
 *
 * @return std::vector<std::string> A list of discovered collection names.
 */
std::vector<std::string> Engine::list_collections()
{
    std::vector<std::string> collections;

    // Safety check: ensure directory exists before iterating
    if (fs::exists(base_path_)) {
        for (const auto& entry : fs::directory_iterator(base_path_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".aev") {
                collections.push_back(entry.path().stem().string());
            }
        }
    }
    return collections;
}

/**
 * @brief Replays the binary log file to reconstruct the in-memory state.
 *
 * Implements a robust frame-based reader loop:
 * 1. **Header Read:** Reads a 4-byte integer to determine payload size.
 * 2. **Allocation:** Pre-allocates the exact buffer size needed.
 * 3. **Body Read:** Reads the payload directly into the buffer.
 *
 * @param collection The name of the collection to load.
 * @return std::vector<std::string> A vector of reconstructed JSON strings.
 */
std::vector<std::string> Engine::load_log(const std::string& collection)
{
    std::vector<std::string> logs;
    std::string path = get_path(collection);

    // Open in Binary mode is critical for frame accuracy.
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) {
        return logs;
    }

    // Peak at the next character to check for EOF before attempting a read
    while (file.peek() != EOF) {
        uint32_t payload_length = 0;

        // 1. Read the 4-byte Length Header
        file.read(reinterpret_cast<char*>(&payload_length), sizeof(payload_length));

        // Validation: If we can't read the full header (file corruption or EOF), stop.
        if (file.gcount() < static_cast<std::streamsize>(sizeof(payload_length))) {
            break;
        }

        // 2. Read the Data Payload
        std::string buffer;
        buffer.resize(payload_length);
        file.read(&buffer[0], payload_length);

        // Validation: Ensure the full payload was read successfully.
        if (file.gcount() == static_cast<std::streamsize>(payload_length)) {
            logs.push_back(std::move(buffer));
        } else {
            // Log corruption detected: Partial frame found.
            // In a production system, we might log a warning here.
            break;
        }
    }
    return logs;
}

/**
 * @brief Persists a document to disk using binary framing.
 *
 * Appends data in the format: `[Length (4 bytes)] [JSON Data (N bytes)]`
 * This allows the reader to skip delimiters and handle binary data safely.
 *
 * @param collection The target collection.
 * @param raw_json The document string.
 * @return true if the write operation committed successfully.
 */
bool Engine::append(const std::string& collection, const std::string& raw_json)
{
    std::string path = get_path(collection);

    // Open in Append + Binary mode.
    std::ofstream file(path, std::ios::binary | std::ios::app);

    if (!file.is_open()) {
        return false;
    }

    // Cast size to fixed-width integer to ensure platform consistency (32-bit).
    uint32_t length = static_cast<uint32_t>(raw_json.size());

    // 1. Write Header (Little Endian on standard x86/x64/ARM64 systems)
    file.write(reinterpret_cast<const char*>(&length), sizeof(length));

    // 2. Write Data Payload
    file.write(raw_json.c_str(), length);

    // Check stream state to ensure physical write success
    return file.good();
}

/**
 * @brief Performs atomic compaction (Garbage Collection) of the log file.
 *
 * **Compaction Strategy:**
 * 1. **Snapshot:** Writes only the *currently active* documents to a temporary file.
 * 2. **Flush:** Ensures all data is physically written to the temp file.
 * 3. **Atomic Swap:** Uses `fs::rename` to replace the old log with the new compacted one.
 * This ensures that at no point is the database in a missing or half-written state.
 *
 * @param collection The collection name.
 * @param active_docs List of valid document snapshots.
 * @return true if successful.
 */
bool Engine::compact(const std::string& collection, const std::vector<std::string>& active_docs)
{
    std::string path = get_path(collection);
    std::string temp_path = path + ".tmp";

    // Open temporary file in Truncate + Binary mode.
    std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);

    if (!file.is_open()) {
        return false;
    }

    // Serialize active documents into the new log
    for (const auto& doc : active_docs) {
        uint32_t length = static_cast<uint32_t>(doc.size());
        file.write(reinterpret_cast<const char*>(&length), sizeof(length));
        file.write(doc.c_str(), length);
    }

    file.flush();
    file.close(); // Close file handle to release lock before renaming

    if (file.fail()) {
        // If writing failed, do not swap. Delete temp file.
        fs::remove(temp_path);
        return false;
    }

    // Atomic filesystem swap
    try {
        fs::rename(temp_path, path);
        return true;
    } catch (const fs::filesystem_error&) {
        return false;
    }
}

} // namespace aevum::storage
